//High frequency tasks made by xTaskCreate()
//And any low frequency tasks that are called by Ticker

volatile static uint16_t dataHead = 0; //Head advances as data comes in from GNSS's UART
volatile int availableHandlerSpace = 0; //settings.gnssHandlerBufferSize - usedSpace

//If the phone has any new data (NTRIP RTCM, etc), read it in over Bluetooth and pass along to ZED
//Scan for escape characters to enter config menu
void F9PSerialWriteTask(void *e)
{
  while (true)
  {
    //Receive RTCM corrections or UBX config messages over bluetooth and pass along to ZED
    if (bluetoothGetState() == BT_CONNECTED)
    {
      while (btPrintEcho == false && bluetoothRxDataAvailable())
      {
        //Check stream for command characters
        byte incoming = bluetoothRead();

        if (incoming == btEscapeCharacter)
        {
          //Ignore escape characters received within 2 seconds of serial traffic
          //Allow escape characters received within first 2 seconds of power on
          if (millis() - btLastByteReceived > btMinEscapeTime || millis() < btMinEscapeTime)
          {
            btEscapeCharsReceived++;
            if (btEscapeCharsReceived == btMaxEscapeCharacters)
            {
              printEndpoint = PRINT_ENDPOINT_ALL;
              systemPrintln("Echoing all serial to BT device");
              btPrintEcho = true;

              btEscapeCharsReceived = 0;
              btLastByteReceived = millis();
            }
          }
          else
          {
            //Ignore this escape character, passing along to output
            serialGNSS.write(incoming);
          }
        }
        else //This is just a character in the stream, ignore
        {
          //Pass any escape characters that turned out to not be a complete escape sequence
          while (btEscapeCharsReceived-- > 0)
            serialGNSS.write(btEscapeCharacter);

          //Pass byte to GNSS receiver or to system
          //TODO - control if this RTCM source should be listened to or not
          serialGNSS.write(incoming);

          btLastByteReceived = millis();
          btEscapeCharsReceived = 0; //Update timeout check for escape char and partial frame

          bluetoothIncomingRTCM = true;
        } //End just a character in the stream

      } //End btPrintEcho == false && bluetoothRxDataAvailable()

    } //End bluetoothGetState() == BT_CONNECTED

    if (settings.enableTaskReports == true)
      systemPrintf("SerialWriteTask High watermark: %d\r\n",  uxTaskGetStackHighWaterMark(NULL));

    delay(1); //Poor man's way of feeding WDT. Required to prevent Priority 1 tasks from causing WDT reset
    taskYIELD();
  } //End while(true)
}

//----------------------------------------------------------------------
//The ESP32<->ZED-F9P serial connection is default 230,400bps to facilitate
//10Hz fix rate with PPP Logging Defaults (NMEAx5 + RXMx2) messages enabled.
//ESP32 UART2 is begun with settings.uartReceiveBufferSize size buffer. The circular buffer
//is 1024*6. At approximately 46.1K characters/second, a 6144 * 2
//byte buffer should hold 267ms worth of serial data. Assuming SD writes are
//250ms worst case, we should record incoming all data. Bluetooth congestion
//or conflicts with the SD card semaphore should clear within this time.
//
//Ring buffer empty when (dataHead == btTail) and (dataHead == sdTail)
//
//        +---------+
//        |         |
//        |         |
//        |         |
//        |         |
//        +---------+ <-- dataHead, btTail, sdTail
//
//Ring buffer contains data when (dataHead != btTail) or (dataHead != sdTail)
//
//        +---------+
//        |         |
//        |         |
//        | yyyyyyy | <-- dataHead
//        | xxxxxxx | <-- btTail (1 byte in buffer)
//        +---------+ <-- sdTail (2 bytes in buffer)
//
//        +---------+
//        | yyyyyyy | <-- btTail (1 byte in buffer)
//        | xxxxxxx | <-- sdTail (2 bytes in buffer)
//        |         |
//        |         |
//        +---------+ <-- dataHead
//
//Maximum ring buffer fill is settings.gnssHandlerBufferSize - 1
//----------------------------------------------------------------------

//Read bytes from ZED-F9P UART1 into ESP32 circular buffer
//If data is coming in at 230,400bps = 23,040 bytes/s = one byte every 0.043ms
//If SD blocks for 150ms (not extraordinary) that is 3,488 bytes that must be buffered
//The ESP32 Arduino FIFO is ~120 bytes by default but overridden to 50 bytes (see pinUART2Task() and uart_set_rx_full_threshold()).
//We use this task to harvest from FIFO into circular buffer during SD write blocking time.
void F9PSerialReadTask(void *e)
{
  static PARSE_STATE parse = {waitForPreamble, processUart1Message, "Log"};

  uint8_t incomingData = 0;

  availableHandlerSpace = settings.gnssHandlerBufferSize;

  while (true)
  {
    if (settings.enableTaskReports == true)
      systemPrintf("SerialReadTask High watermark: %d\r\n", uxTaskGetStackHighWaterMark(NULL));

    //Determine if serial data is available
    while (serialGNSS.available())
    {
      //Read the data from UART1
      incomingData = serialGNSS.read();

      //Save the data byte
      parse.buffer[parse.length++] = incomingData;
      parse.length %= PARSE_BUFFER_LENGTH;

      //Compute the CRC value for the message
      if (parse.computeCrc)
        parse.crc = COMPUTE_CRC24Q(&parse, incomingData);

      //Update the parser state based on the incoming byte
      parse.state(&parse, incomingData);
    }

    delay(1);
    taskYIELD();
  }
}

//Process a complete message incoming from parser
//If we get a complete NMEA/UBX/RTCM sentence, pass on to SD/BT/TCP interfaces
void processUart1Message(PARSE_STATE * parse, uint8_t type)
{
  uint16_t bytesToCopy;
  uint16_t remainingBytes;

  //Display the message
  if (settings.enablePrintLogFileMessages && (!parse->crc) && (!inMainMenu))
  {
    printTimeStamp();
    switch (type)
    {
      case SENTENCE_TYPE_NMEA:
        systemPrintf ("    %s NMEA %s, %2d bytes\r\n", parse->parserName,
                      parse->nmeaMessageName, parse->length);
        break;

      case SENTENCE_TYPE_RTCM:
        systemPrintf ("    %s RTCM %d, %2d bytes\r\n", parse->parserName,
                      parse->message, parse->length);
        break;

      case SENTENCE_TYPE_UBX:
        systemPrintf ("    %s UBX %d.%d, %2d bytes\r\n", parse->parserName,
                      parse->message >> 8, parse->message & 0xff, parse->length);
        break;
    }
  }

  //Determine if this message will fit into the ring buffer
  bytesToCopy = parse->length;
  if ((bytesToCopy > availableHandlerSpace) && (!inMainMenu))
  {
    systemPrintf("Ring buffer full, discarding %d bytes\r\n", bytesToCopy);
    return;
  }

  //Account for this message
  availableHandlerSpace -= bytesToCopy;

  //Fill the buffer to the end and then start at the beginning
  if ((dataHead + bytesToCopy) > settings.gnssHandlerBufferSize)
    bytesToCopy = settings.gnssHandlerBufferSize - dataHead;

  //Display the dataHead offset
  if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
    systemPrintf("DH: %4d --> ", dataHead);

  //Copy the data into the ring buffer
  memcpy(&ringBuffer[dataHead], parse->buffer, bytesToCopy);
  dataHead += bytesToCopy;
  if (dataHead >= settings.gnssHandlerBufferSize)
    dataHead -= settings.gnssHandlerBufferSize;

  //Determine the remaining bytes
  remainingBytes = parse->length - bytesToCopy;
  if (remainingBytes)
  {
    //Copy the remaining bytes into the beginning of the ring buffer
    memcpy(ringBuffer, &parse->buffer[bytesToCopy], remainingBytes);
    dataHead += remainingBytes;
    if (dataHead >= settings.gnssHandlerBufferSize)
      dataHead -= settings.gnssHandlerBufferSize;
  }

  //Display the dataHead offset
  if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
    systemPrintf("%4d\r\n", dataHead);
}

//If new data is in the ringBuffer, dole it out to appropriate interface
//Send data out Bluetooth, record to SD, or send over TCP
void handleGNSSDataTask(void *e)
{
  volatile static uint16_t btTail = 0; //BT Tail advances as it is sent over BT
  volatile static uint16_t tcpTail = 0; //TCP client tail
  volatile static uint16_t sdTail = 0; //SD Tail advances as it is recorded to SD

  int btBytesToSend; //Amount of buffered Bluetooth data
  int tcpBytesToSend; //Amount of buffered TCP data
  int sdBytesToRecord; //Amount of buffered microSD card logging data

  int btConnected; //Is the device in a state to send Bluetooth data?

  while (true)
  {
    //Determine the amount of Bluetooth data in the buffer
    btBytesToSend = 0;

    //Determine BT connection state
    btConnected = (bluetoothGetState() == BT_CONNECTED)
                  && (systemState != STATE_BASE_TEMP_SETTLE)
                  && (systemState != STATE_BASE_TEMP_SURVEY_STARTED);

    if (btConnected)
    {
      btBytesToSend = dataHead - btTail;
      if (btBytesToSend < 0)
        btBytesToSend += settings.gnssHandlerBufferSize;
    }

    //Determine the amount of TCP data in the buffer
    tcpBytesToSend = 0;
    if (settings.enableTcpServer || settings.enableTcpClient)
    {
      tcpBytesToSend = dataHead - tcpTail;
      if (tcpBytesToSend < 0)
        tcpBytesToSend += settings.gnssHandlerBufferSize;
    }

    //Determine the amount of microSD card logging data in the buffer
    sdBytesToRecord = 0;
    if (online.logging)
    {
      sdBytesToRecord = dataHead - sdTail;
      if (sdBytesToRecord < 0)
        sdBytesToRecord += settings.gnssHandlerBufferSize;
    }

    //----------------------------------------------------------------------
    //Send data over Bluetooth
    //----------------------------------------------------------------------

    if (!btConnected)
      //Discard the data
      btTail = dataHead;
    else if (btBytesToSend > 0)
    {
      //Reduce bytes to send if we have more to send then the end of the buffer
      //We'll wrap next loop
      if ((btTail + btBytesToSend) > settings.gnssHandlerBufferSize)
        btBytesToSend = settings.gnssHandlerBufferSize - btTail;

      //If we are in the config menu, supress data flowing from ZED to cell phone
      if (btPrintEcho == false)
        //Push new data to BT SPP
        btBytesToSend = bluetoothWrite(&ringBuffer[btTail], btBytesToSend);

      if (btBytesToSend > 0)
      {
        //If we are in base mode, assume part of the outgoing data is RTCM
        if (systemState >= STATE_BASE_NOT_STARTED && systemState <= STATE_BASE_FIXED_TRANSMITTING)
          bluetoothOutgoingRTCM = true;

        //Account for the sent or dropped data
        btTail += btBytesToSend;
        if (btTail >= settings.gnssHandlerBufferSize)
          btTail -= settings.gnssHandlerBufferSize;
      }
      else
        log_w("BT failed to send");
    }

    //----------------------------------------------------------------------
    //Send data to the TCP clients
    //----------------------------------------------------------------------

    if ((!settings.enableTcpServer) && (!settings.enableTcpClient) && (!wifiTcpConnected))
      tcpTail = dataHead;
    else if (tcpBytesToSend > 0)
    {
      //Reduce bytes to send if we have more to send then the end of the buffer
      //We'll wrap next loop
      if ((tcpTail + tcpBytesToSend) > settings.gnssHandlerBufferSize)
        tcpBytesToSend = settings.gnssHandlerBufferSize - tcpTail;

      //Send the data to the TCP clients
      wifiSendTcpData(&ringBuffer[tcpTail], tcpBytesToSend);

      //Assume all data was sent, wrap the buffer pointer
      tcpTail += tcpBytesToSend;
      if (tcpTail >= settings.gnssHandlerBufferSize)
        tcpTail -= settings.gnssHandlerBufferSize;
    }

    //----------------------------------------------------------------------
    //Log data to the SD card
    //----------------------------------------------------------------------

    //If user wants to log, record to SD
    if (!online.logging)
      //Discard the data
      sdTail = dataHead;
    else if (sdBytesToRecord > 0)
    {
      //Check if we are inside the max time window for logging
      if ((systemTime_minutes - startLogTime_minutes) < settings.maxLogTime_minutes)
      {
        //Attempt to gain access to the SD card, avoids collisions with file
        //writing from other functions like recordSystemSettingsToFile()
        if (xSemaphoreTake(sdCardSemaphore, loggingSemaphoreWait_ms) == pdPASS)
        {
          markSemaphore(FUNCTION_WRITESD);

          //Reduce bytes to record if we have more then the end of the buffer
          int sliceToRecord = sdBytesToRecord;
          if ((sdTail + sliceToRecord) > settings.gnssHandlerBufferSize)
            sliceToRecord = settings.gnssHandlerBufferSize - sdTail;

          if (settings.enablePrintSDBuffers && !inMainMenu)
          {
            int availableUARTSpace = settings.uartReceiveBufferSize - serialGNSS.available();
            systemPrintf("SD Incoming Serial: %04d\tToRead: %04d\tMovedToBuffer: %04d\tavailableUARTSpace: %04d\tavailableHandlerSpace: %04d\tToRecord: %04d\tRecorded: %04d\tBO: %d\r\n", serialGNSS.available(), 0, 0, availableUARTSpace, availableHandlerSpace, sliceToRecord, 0, bufferOverruns);
          }

          //Write the data to the file
          long startTime = millis();
          sdBytesToRecord = ubxFile->write(&ringBuffer[sdTail], sliceToRecord);

          fileSize = ubxFile->fileSize(); //Update file size
          sdFreeSpace -= sliceToRecord; //Update remaining space on SD

          //Force file sync every 60s
          if (millis() - lastUBXLogSyncTime > 60000)
          {
            if (productVariant == RTK_SURVEYOR)
              digitalWrite(pin_baseStatusLED, !digitalRead(pin_baseStatusLED)); //Blink LED to indicate logging activity

            ubxFile->sync();
            updateDataFileAccess(ubxFile); // Update the file access time & date
            if (productVariant == RTK_SURVEYOR)
              digitalWrite(pin_baseStatusLED, !digitalRead(pin_baseStatusLED)); //Return LED to previous state

            lastUBXLogSyncTime = millis();
          }

          long endTime = millis();

          if (settings.enablePrintBufferOverrun)
          {
            if (endTime - startTime > 150)
              systemPrintf("Long Write! Time: %ld ms / Location: %ld / Recorded %d bytes / spaceRemaining %d bytes\r\n", endTime - startTime, fileSize, sdBytesToRecord, combinedSpaceRemaining);
          }

          xSemaphoreGive(sdCardSemaphore);

          //Account for the sent data or dropped
          if (sdBytesToRecord > 0)
          {
            sdTail += sdBytesToRecord;
            if (sdTail >= settings.gnssHandlerBufferSize)
              sdTail -= settings.gnssHandlerBufferSize;
          }
        } //End sdCardSemaphore
        else
        {
          char semaphoreHolder[50];
          getSemaphoreFunction(semaphoreHolder);
          log_w("sdCardSemaphore failed to yield for SD write, held by %s, Tasks.ino line %d", semaphoreHolder, __LINE__);
        }
      } //End maxLogTime
    } //End logging

    //Update space available for use in UART task
    btBytesToSend = dataHead - btTail;
    if (btBytesToSend < 0)
      btBytesToSend += settings.gnssHandlerBufferSize;

    tcpBytesToSend = dataHead - tcpTail;
    if (tcpBytesToSend < 0)
      tcpBytesToSend += settings.gnssHandlerBufferSize;

    sdBytesToRecord = dataHead - sdTail;
    if (sdBytesToRecord < 0)
      sdBytesToRecord += settings.gnssHandlerBufferSize;

    //Determine the inteface that is most behind: SD writing, SPP transmission, or TCP transmission
    int usedSpace = 0;
    if (tcpBytesToSend >= btBytesToSend && tcpBytesToSend >= sdBytesToRecord)
      usedSpace = tcpBytesToSend;
    else if (btBytesToSend >= sdBytesToRecord && btBytesToSend >= tcpBytesToSend)
      usedSpace = btBytesToSend;
    else
      usedSpace = sdBytesToRecord;

    availableHandlerSpace = settings.gnssHandlerBufferSize - usedSpace;

    //Don't fill the last byte to prevent buffer overflow
    if (availableHandlerSpace)
      availableHandlerSpace -= 1;

    //----------------------------------------------------------------------
    //Let other tasks run, prevent watch dog timer (WDT) resets
    //----------------------------------------------------------------------

    delay(1);
    taskYIELD();
  }
}

//Control BT status LED according to bluetoothGetState()
void updateBTled()
{
  if (productVariant == RTK_SURVEYOR)
  {
    //Blink on/off while we wait for BT connection
    if (bluetoothGetState() == BT_NOTCONNECTED)
    {
      if (btFadeLevel == 0) btFadeLevel = 255;
      else btFadeLevel = 0;
      ledcWrite(ledBTChannel, btFadeLevel);
    }

    //Solid LED if BT Connected
    else if (bluetoothGetState() == BT_CONNECTED)
      ledcWrite(ledBTChannel, 255);

    //Pulse LED while no BT and we wait for WiFi connection
    else if (wifiState == WIFI_CONNECTING || wifiState == WIFI_CONNECTED)
    {
      //Fade in/out the BT LED during WiFi AP mode
      btFadeLevel += pwmFadeAmount;
      if (btFadeLevel <= 0 || btFadeLevel >= 255) pwmFadeAmount *= -1;

      if (btFadeLevel > 255) btFadeLevel = 255;
      if (btFadeLevel < 0) btFadeLevel = 0;

      ledcWrite(ledBTChannel, btFadeLevel);
    }
    else
      ledcWrite(ledBTChannel, 0);
  }
}

//For RTK Express and RTK Facet, monitor momentary buttons
void ButtonCheckTask(void *e)
{
  uint8_t index;

  if (setupBtn != NULL) setupBtn->begin();
  if (powerBtn != NULL) powerBtn->begin();

  while (true)
  {
    /* RTK Surveyor

                                  .----------------------------.
                                  |                            |
                                  V                            |
                        .------------------.                   |
                        |     Power On     |                   |
                        '------------------'                   |
                                  |                            |
                                  | Setup button = 0           |
                                  V                            |
                        .------------------.                   |
                .------>|    Rover Mode    |                   |
                |       '------------------'                   |
                |                 |                            |
                |                 | Setup button = 1           |
                |                 V                            |
                |       .------------------.                   |
                '-------|    Base Mode     |                   |
       Setup button = 0 '------------------'                   |
       after long time    |             |                      |
                          |             | Setup button = 0     |
         Setup button = 0 |             | after short time     |
         after short time |             | (< 500 mSec)         |
             (< 500 mSec) |             |                      |
      STATE_ROVER_NOT_STARTED |             |                      |
                          V             V                      |
          .------------------.   .------------------.          |
          |    Test Mode     |   | WiFi Config Mode |----------'
          '------------------'   '------------------'

    */

    if (productVariant == RTK_SURVEYOR)
    {
      setupBtn->read();

      //When switch is set to '1' = BASE, pin will be shorted to ground
      if (setupBtn->isPressed()) //Switch is set to base mode
      {
        if (buttonPreviousState == BUTTON_ROVER)
        {
          lastRockerSwitchChange = millis(); //Record for WiFi AP access
          buttonPreviousState = BUTTON_BASE;
          requestChangeState(STATE_BASE_NOT_STARTED);
        }
      }
      else if (setupBtn->wasReleased()) //Switch is set to Rover
      {
        if (buttonPreviousState == BUTTON_BASE)
        {
          buttonPreviousState = BUTTON_ROVER;

          //If quick toggle is detected (less than 500ms), enter WiFi AP Config mode
          if (millis() - lastRockerSwitchChange < 500)
          {
            if (systemState == STATE_ROVER_NOT_STARTED && online.display == true) //Catch during Power On
              requestChangeState(STATE_TEST); //If RTK Surveyor, with display attached, during Rover not started, then enter test mode
            else
              requestChangeState(STATE_WIFI_CONFIG_NOT_STARTED);
          }
          else
          {
            requestChangeState(STATE_ROVER_NOT_STARTED);
          }
        }
      }
    }
    else if (productVariant == RTK_EXPRESS || productVariant == RTK_EXPRESS_PLUS) //Express: Check both of the momentary switches
    {
      if (setupBtn != NULL) setupBtn->read();
      if (powerBtn != NULL) powerBtn->read();

      if (systemState == STATE_SHUTDOWN)
      {
        //Ignore button presses while shutting down
      }
      else if (powerBtn != NULL && powerBtn->pressedFor(shutDownButtonTime))
      {
        forceSystemStateUpdate = true;
        requestChangeState(STATE_SHUTDOWN);

        if (inMainMenu) powerDown(true); //State machine is not updated while in menu system so go straight to power down as needed
      }
      else if ((setupBtn != NULL && setupBtn->pressedFor(500)) &&
               (powerBtn != NULL && powerBtn->pressedFor(500)))
      {
        forceSystemStateUpdate = true;
        requestChangeState(STATE_TEST);
        lastTestMenuChange = millis(); //Avoid exiting test menu for 1s
      }
      else if (setupBtn != NULL && setupBtn->wasReleased())
      {
        switch (systemState)
        {
          //If we are in any running state, change to STATE_DISPLAY_SETUP
          case STATE_ROVER_NOT_STARTED:
          case STATE_ROVER_NO_FIX:
          case STATE_ROVER_FIX:
          case STATE_ROVER_RTK_FLOAT:
          case STATE_ROVER_RTK_FIX:
          case STATE_BASE_NOT_STARTED:
          case STATE_BASE_TEMP_SETTLE:
          case STATE_BASE_TEMP_SURVEY_STARTED:
          case STATE_BASE_TEMP_TRANSMITTING:
          case STATE_BASE_FIXED_NOT_STARTED:
          case STATE_BASE_FIXED_TRANSMITTING:
          case STATE_BUBBLE_LEVEL:
          case STATE_WIFI_CONFIG_NOT_STARTED:
          case STATE_WIFI_CONFIG:
          case STATE_ESPNOW_PAIRING_NOT_STARTED:
          case STATE_ESPNOW_PAIRING:
            lastSystemState = systemState; //Remember this state to return after we mark an event or ESP-Now pair
            requestChangeState(STATE_DISPLAY_SETUP);
            setupState = STATE_MARK_EVENT;
            lastSetupMenuChange = millis();
            break;

          case STATE_MARK_EVENT:
            //If the user presses the setup button during a mark event, do nothing
            //Allow system to return to lastSystemState
            break;

          case STATE_PROFILE:
            //If the user presses the setup button during a profile change, do nothing
            //Allow system to return to lastSystemState
            break;

          case STATE_TEST:
            //Do nothing. User is releasing the setup button.
            break;

          case STATE_TESTING:
            //If we are in testing, return to Rover Not Started
            requestChangeState(STATE_ROVER_NOT_STARTED);
            break;

          case STATE_DISPLAY_SETUP:
            //If we are displaying the setup menu, cycle through possible system states
            //Exit display setup and enter new system state after ~1500ms in updateSystemState()
            lastSetupMenuChange = millis();

            forceDisplayUpdate = true; //User is interacting so repaint display quickly

            switch (setupState)
            {
              case STATE_MARK_EVENT:
                setupState = STATE_ROVER_NOT_STARTED;
                break;
              case STATE_ROVER_NOT_STARTED:
                //If F9R, skip base state
                if (zedModuleType == PLATFORM_F9R)
                  setupState = STATE_BUBBLE_LEVEL;
                else
                  setupState = STATE_BASE_NOT_STARTED;
                break;
              case STATE_BASE_NOT_STARTED:
                setupState = STATE_BUBBLE_LEVEL;
                break;
              case STATE_BUBBLE_LEVEL:
                setupState = STATE_WIFI_CONFIG_NOT_STARTED;
                break;
              case STATE_WIFI_CONFIG_NOT_STARTED:
                setupState = STATE_ESPNOW_PAIRING_NOT_STARTED;
                break;
              case STATE_ESPNOW_PAIRING_NOT_STARTED:
                //If only one active profile do not show any profiles
                index = getProfileNumberFromUnit(0);
                displayProfile = getProfileNumberFromUnit(1);
                setupState = (index >= displayProfile) ? STATE_MARK_EVENT : STATE_PROFILE;
                displayProfile = 0;
                break;
              case STATE_PROFILE:
                //Done when no more active profiles
                displayProfile++;
                if (!getProfileNumberFromUnit(displayProfile))
                  setupState = STATE_MARK_EVENT;
                break;
              default:
                systemPrintf("ButtonCheckTask unknown setup state: %d\r\n", setupState);
                setupState = STATE_MARK_EVENT;
                break;
            }
            break;

          default:
            systemPrintf("ButtonCheckTask unknown system state: %d\r\n", systemState);
            requestChangeState(STATE_ROVER_NOT_STARTED);
            break;
        }
      }
    } //End Platform = RTK Express
    else if (productVariant == RTK_FACET || productVariant == RTK_FACET_LBAND) //Check one momentary button
    {
      if (powerBtn != NULL) powerBtn->read();

      if (systemState == STATE_SHUTDOWN)
      {
        //Ignore button presses while shutting down
      }
      else if (powerBtn != NULL && powerBtn->pressedFor(shutDownButtonTime))
      {
        forceSystemStateUpdate = true;
        requestChangeState(STATE_SHUTDOWN);

        if (inMainMenu) powerDown(true); //State machine is not updated while in menu system so go straight to power down as needed
      }
      else if (powerBtn != NULL && systemState == STATE_ROVER_NOT_STARTED && firstRoverStart == true && powerBtn->pressedFor(500))
      {
        forceSystemStateUpdate = true;
        requestChangeState(STATE_TEST);
        lastTestMenuChange = millis(); //Avoid exiting test menu for 1s
      }
      else if (powerBtn != NULL && powerBtn->wasReleased() && firstRoverStart == false)
      {
        switch (systemState)
        {
          //If we are in any running state, change to STATE_DISPLAY_SETUP
          case STATE_ROVER_NOT_STARTED:
          case STATE_ROVER_NO_FIX:
          case STATE_ROVER_FIX:
          case STATE_ROVER_RTK_FLOAT:
          case STATE_ROVER_RTK_FIX:
          case STATE_BASE_NOT_STARTED:
          case STATE_BASE_TEMP_SETTLE:
          case STATE_BASE_TEMP_SURVEY_STARTED:
          case STATE_BASE_TEMP_TRANSMITTING:
          case STATE_BASE_FIXED_NOT_STARTED:
          case STATE_BASE_FIXED_TRANSMITTING:
          case STATE_BUBBLE_LEVEL:
          case STATE_WIFI_CONFIG_NOT_STARTED:
          case STATE_WIFI_CONFIG:
          case STATE_ESPNOW_PAIRING_NOT_STARTED:
          case STATE_ESPNOW_PAIRING:
            lastSystemState = systemState; //Remember this state to return after we mark an event or ESP-Now pair
            requestChangeState(STATE_DISPLAY_SETUP);
            setupState = STATE_MARK_EVENT;
            lastSetupMenuChange = millis();
            break;

          case STATE_MARK_EVENT:
            //If the user presses the setup button during a mark event, do nothing
            //Allow system to return to lastSystemState
            break;

          case STATE_PROFILE:
            //If the user presses the setup button during a profile change, do nothing
            //Allow system to return to lastSystemState
            break;

          case STATE_TEST:
            //Do nothing. User is releasing the setup button.
            break;

          case STATE_TESTING:
            //If we are in testing, return to Rover Not Started
            requestChangeState(STATE_ROVER_NOT_STARTED);
            break;

          case STATE_DISPLAY_SETUP:
            //If we are displaying the setup menu, cycle through possible system states
            //Exit display setup and enter new system state after ~1500ms in updateSystemState()
            lastSetupMenuChange = millis();

            forceDisplayUpdate = true; //User is interacting so repaint display quickly

            switch (setupState)
            {
              case STATE_MARK_EVENT:
                setupState = STATE_ROVER_NOT_STARTED;
                break;
              case STATE_ROVER_NOT_STARTED:
                //If F9R, skip base state
                if (zedModuleType == PLATFORM_F9R)
                  setupState = STATE_BUBBLE_LEVEL;
                else
                  setupState = STATE_BASE_NOT_STARTED;
                break;
              case STATE_BASE_NOT_STARTED:
                setupState = STATE_BUBBLE_LEVEL;
                break;
              case STATE_BUBBLE_LEVEL:
                setupState = STATE_WIFI_CONFIG_NOT_STARTED;
                break;
              case STATE_WIFI_CONFIG_NOT_STARTED:
                setupState = STATE_ESPNOW_PAIRING_NOT_STARTED;
                break;
              case STATE_ESPNOW_PAIRING_NOT_STARTED:
                //If only one active profile do not show any profiles
                index = getProfileNumberFromUnit(0);
                displayProfile = getProfileNumberFromUnit(1);
                setupState = (index >= displayProfile) ? STATE_MARK_EVENT : STATE_PROFILE;
                displayProfile = 0;
                break;
              case STATE_PROFILE:
                //Done when no more active profiles
                displayProfile++;
                if (!getProfileNumberFromUnit(displayProfile))
                  setupState = STATE_MARK_EVENT;
                break;
              default:
                systemPrintf("ButtonCheckTask unknown setup state: %d\r\n", setupState);
                setupState = STATE_MARK_EVENT;
                break;
            }
            break;

          default:
            systemPrintf("ButtonCheckTask unknown system state: %d\r\n", systemState);
            requestChangeState(STATE_ROVER_NOT_STARTED);
            break;
        }
      }
    } //End Platform = RTK Facet

    delay(1); //Poor man's way of feeding WDT. Required to prevent Priority 1 tasks from causing WDT reset
    taskYIELD();
  }
}

void idleTask(void *e)
{
  int cpu = xPortGetCoreID();
  uint32_t idleCount = 0;
  uint32_t lastDisplayIdleTime = 0;
  uint32_t lastStackPrintTime = 0;

  while (1)
  {
    //Increment a count during the idle time
    idleCount++;

    //Determine if it is time to print the CPU idle times
    if ((millis() - lastDisplayIdleTime) >= (IDLE_TIME_DISPLAY_SECONDS * 1000))
    {
      lastDisplayIdleTime = millis();

      //Get the idle time
      if (idleCount > max_idle_count)
        max_idle_count = idleCount;

      //Display the idle times
      if (settings.enablePrintIdleTime) {
        systemPrintf("CPU %d idle time: %d%% (%d/%d)\r\n", cpu,
                     idleCount * 100 / max_idle_count,
                     idleCount, max_idle_count);

        //Print the task count
        if (cpu)
          systemPrintf("%d Tasks\r\n", uxTaskGetNumberOfTasks());
      }

      //Restart the idle count for the next display time
      idleCount = 0;
    }

    //Display the high water mark if requested
    if ((settings.enableTaskReports == true)
        && ((millis() - lastStackPrintTime) >= (IDLE_TIME_DISPLAY_SECONDS * 1000)))
    {
      lastStackPrintTime = millis();
      systemPrintf("idleTask %d High watermark: %d\r\n",
                   xPortGetCoreID(), uxTaskGetStackHighWaterMark(NULL));
    }

    //Let other same priority tasks run
    taskYIELD();
  }
}

//Serial Read/Write tasks for the F9P must be started after BT is up and running otherwise SerialBT->available will cause reboot
void tasksStartUART2()
{
  //Reads data from ZED and stores data into circular buffer
  if (F9PSerialReadTaskHandle == NULL)
    xTaskCreate(
      F9PSerialReadTask, //Function to call
      "F9Read", //Just for humans
      readTaskStackSize, //Stack Size
      NULL, //Task input parameter
      F9PSerialReadTaskPriority, //Priority
      &F9PSerialReadTaskHandle); //Task handle

  //Reads data from circular buffer and sends data to SD, SPP, or TCP
  if (handleGNSSDataTaskHandle == NULL)
    xTaskCreate(
      handleGNSSDataTask, //Function to call
      "handleGNSSData", //Just for humans
      handleGNSSDataTaskStackSize, //Stack Size
      NULL, //Task input parameter
      handleGNSSDataTaskPriority, //Priority
      &handleGNSSDataTaskHandle); //Task handle

  //Reads data from BT and sends to ZED
  if (F9PSerialWriteTaskHandle == NULL)
    xTaskCreate(
      F9PSerialWriteTask, //Function to call
      "F9Write", //Just for humans
      writeTaskStackSize, //Stack Size
      NULL, //Task input parameter
      F9PSerialWriteTaskPriority, //Priority
      &F9PSerialWriteTaskHandle); //Task handle
}

//Stop tasks - useful when running firmware update or WiFi AP is running
void tasksStopUART2()
{
  //Delete tasks if running
  if (F9PSerialReadTaskHandle != NULL)
  {
    vTaskDelete(F9PSerialReadTaskHandle);
    F9PSerialReadTaskHandle = NULL;
  }
  if (handleGNSSDataTaskHandle != NULL)
  {
    vTaskDelete(handleGNSSDataTaskHandle);
    handleGNSSDataTaskHandle = NULL;
  }
  if (F9PSerialWriteTaskHandle != NULL)
  {
    vTaskDelete(F9PSerialWriteTaskHandle);
    F9PSerialWriteTaskHandle = NULL;
  }

  //Give the other CPU time to finish
  //Eliminates CPU bus hang condition
  delay(100);
}

//Checking the number of available clusters on the SD card can take multiple seconds
//Rather than blocking the system, we run a background task
//Once the size check is complete, the task is removed
void sdSizeCheckTask(void *e)
{
  while (true)
  {
    if (online.microSD && sdCardSize == 0)
    {
      //Attempt to gain access to the SD card
      if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
      {
        markSemaphore(FUNCTION_SDSIZECHECK);

        csd_t csd;
        sd->card()->readCSD(&csd); //Card Specific Data
        sdCardSize = (uint64_t)512 * sd->card()->sectorCount();

        sd->volumeBegin();

        //Find available cluster/space
        sdFreeSpace = sd->vol()->freeClusterCount(); //This takes a few seconds to complete
        sdFreeSpace *= sd->vol()->sectorsPerCluster();
        sdFreeSpace *= 512L; //Bytes per sector

        xSemaphoreGive(sdCardSemaphore);

        //uint64_t sdUsedSpace = sdCardSize - sdFreeSpace; //Don't think of it as used, think of it as unusable

        systemPrintf("SD card size: %s / Free space: %s\r\n",
                 stringHumanReadableSize(sdCardSize),
                 stringHumanReadableSize(sdFreeSpace)
                );

        outOfSDSpace = false;

        sdSizeCheckTaskComplete = true;
      }
      else
      {
        char semaphoreHolder[50];
        getSemaphoreFunction(semaphoreHolder);
        log_d("sdCardSemaphore failed to yield, held by %s, Tasks.ino line %d\r\n", semaphoreHolder, __LINE__);
      }
    }

    delay(1);
    taskYIELD(); //Let other tasks run
  }
}
