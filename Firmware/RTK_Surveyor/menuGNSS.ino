//Configure the basic GNSS reception settings
//Update rate, constellations, etc
void menuGNSS()
{
  restartRover = false; //If user modifies any NTRIP settings, we need to restart the rover

  while (1)
  {
    systemPrintln();
    systemPrintln("Menu: GNSS Receiver");

    //Because we may be in base mode (always 1Hz), do not get freq from module, use settings instead
    float measurementFrequency = (1000.0 / settings.measurementRate) / settings.navigationRate;

    systemPrint("1) Set measurement rate in Hz: ");
    systemPrintln(measurementFrequency, 5);

    systemPrint("2) Set measurement rate in seconds between measurements: ");
    systemPrintln(1 / measurementFrequency, 5);

    systemPrint("3) Set dynamic model: ");
    switch (settings.dynamicModel)
    {
      case DYN_MODEL_PORTABLE:
        systemPrint("Portable");
        break;
      case DYN_MODEL_STATIONARY:
        systemPrint("Stationary");
        break;
      case DYN_MODEL_PEDESTRIAN:
        systemPrint("Pedestrian");
        break;
      case DYN_MODEL_AUTOMOTIVE:
        systemPrint("Automotive");
        break;
      case DYN_MODEL_SEA:
        systemPrint("Sea");
        break;
      case DYN_MODEL_AIRBORNE1g:
        systemPrint("Airborne 1g");
        break;
      case DYN_MODEL_AIRBORNE2g:
        systemPrint("Airborne 2g");
        break;
      case DYN_MODEL_AIRBORNE4g:
        systemPrint("Airborne 4g");
        break;
      case DYN_MODEL_WRIST:
        systemPrint("Wrist");
        break;
      case DYN_MODEL_BIKE:
        systemPrint("Bike");
        break;
      default:
        systemPrint("Unknown");
        break;
    }
    systemPrintln();

    systemPrintln("4) Set Constellations");

    systemPrint("5) Toggle NTRIP Client: ");
    if (settings.enableNtripClient == true) systemPrintln("Enabled");
    else systemPrintln("Disabled");

    if (settings.enableNtripClient == true)
    {
      systemPrint("6) Set Caster Address: ");
      systemPrintln(settings.ntripClient_CasterHost);

      systemPrint("7) Set Caster Port: ");
      systemPrintln(settings.ntripClient_CasterPort);

      systemPrint("8) Set Caster User Name: ");
      systemPrintln(settings.ntripClient_CasterUser);

      systemPrint("9) Set Caster User Password: ");
      systemPrintln(settings.ntripClient_CasterUserPW);

      systemPrint("10) Set Mountpoint: ");
      systemPrintln(settings.ntripClient_MountPoint);

      systemPrint("11) Set Mountpoint PW: ");
      systemPrintln(settings.ntripClient_MountPointPW);

      systemPrint("12) Toggle sending GGA Location to Caster: ");
      if (settings.ntripClient_TransmitGGA == true) systemPrintln("Enabled");
      else systemPrintln("Disabled");
    }

    systemPrintln("x) Exit");

    int incoming = getNumber(); //Returns EXIT, TIMEOUT, or long

    if (incoming == 1)
    {
      systemPrint("Enter GNSS measurement rate in Hz: ");
      double rate = getDouble();
      if (rate < 0.00012 || rate > 20.0) //20Hz limit with all constellations enabled
      {
        systemPrintln("Error: Measurement rate out of range");
      }
      else
      {
        setRate(1.0 / rate); //Convert Hz to seconds. This will set settings.measurementRate, settings.navigationRate, and GSV message
        //Settings recorded to NVM and file at main menu exit
      }
    }
    else if (incoming == 2)
    {
      systemPrint("Enter GNSS measurement rate in seconds between measurements: ");
      float rate = getDouble();
      if (rate < 0.0 || rate > 8255.0) //Limit of 127 (navRate) * 65000ms (measRate) = 137 minute limit.
      {
        systemPrintln("Error: Measurement rate out of range");
      }
      else
      {
        setRate(rate); //This will set settings.measurementRate, settings.navigationRate, and GSV message
        //Settings recorded to NVM and file at main menu exit
      }
    }
    else if (incoming == 3)
    {
      systemPrintln("Enter the dynamic model to use: ");
      systemPrintln("1) Portable");
      systemPrintln("2) Stationary");
      systemPrintln("3) Pedestrian");
      systemPrintln("4) Automotive");
      systemPrintln("5) Sea");
      systemPrintln("6) Airborne 1g");
      systemPrintln("7) Airborne 2g");
      systemPrintln("8) Airborne 4g");
      systemPrintln("9) Wrist");
      systemPrintln("10) Bike");

      int dynamicModel = getNumber(); //Returns EXIT, TIMEOUT, or long
      if ((dynamicModel != INPUT_RESPONSE_GETNUMBER_EXIT) && (dynamicModel != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
      {
        if (dynamicModel < 1 || dynamicModel > DYN_MODEL_BIKE)
          systemPrintln("Error: Dynamic model out of range");
        else
        {
          if (dynamicModel == 1)
            settings.dynamicModel = DYN_MODEL_PORTABLE; //The enum starts at 0 and skips 1.
          else
            settings.dynamicModel = dynamicModel; //Recorded to NVM and file at main menu exit
        }
      }
    }
    else if (incoming == 4)
    {
      menuConstellations();
    }
    else if (incoming == 5)
    {
      settings.enableNtripClient ^= 1;
      restartRover = true;
    }
    else if (incoming == 6 && settings.enableNtripClient == true)
    {
      systemPrint("Enter new Caster Address: ");
      getString(settings.ntripClient_CasterHost, sizeof(settings.ntripClient_CasterHost));
      restartRover = true;
    }
    else if (incoming == 7 && settings.enableNtripClient == true)
    {
      systemPrint("Enter new Caster Port: ");

      int ntripClient_CasterPort = getNumber(); //Returns EXIT, TIMEOUT, or long
      if ((ntripClient_CasterPort != INPUT_RESPONSE_GETNUMBER_EXIT) && (ntripClient_CasterPort != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
      {
        if (ntripClient_CasterPort < 1 || ntripClient_CasterPort > 99999) //Arbitrary 99k max port #
          systemPrintln("Error: Caster port out of range");
        else
          settings.ntripClient_CasterPort = ntripClient_CasterPort; //Recorded to NVM and file at main menu exit
        restartRover = true;
      }
    }
    else if (incoming == 8 && settings.enableNtripClient == true)
    {
      systemPrintf("Enter user name for %s: ", settings.ntripClient_CasterHost);
      getString(settings.ntripClient_CasterUser, sizeof(settings.ntripClient_CasterUser));
      restartRover = true;
    }
    else if (incoming == 9 && settings.enableNtripClient == true)
    {
      systemPrintf("Enter user password for %s: ", settings.ntripClient_CasterHost);
      getString(settings.ntripClient_CasterUserPW, sizeof(settings.ntripClient_CasterUserPW));
      restartRover = true;
    }
    else if (incoming == 10 && settings.enableNtripClient == true)
    {
      systemPrint("Enter new Mount Point: ");
      getString(settings.ntripClient_MountPoint, sizeof(settings.ntripClient_MountPoint));
      restartRover = true;
    }
    else if (incoming == 11 && settings.enableNtripClient == true)
    {
      systemPrintf("Enter password for Mount Point %s: ", settings.ntripClient_MountPoint);
      getString(settings.ntripClient_MountPointPW, sizeof(settings.ntripClient_MountPointPW));
      restartRover = true;
    }
    else if (incoming == 12 && settings.enableNtripClient == true)
    {
      settings.ntripClient_TransmitGGA ^= 1;
      restartRover = true;
    }
    else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
      break;
    else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
      break;
    else
      printUnknown(incoming);
  }

  //Error check for RTK2Go without email in user name
  //First force tolower the host name
  char lowerHost[50];
  strcpy(lowerHost, settings.ntripClient_CasterHost);
  for (int x = 0 ; x < 50 ; x++)
  {
    if (lowerHost[x] == '\0') break;
    if (lowerHost[x] >= 'A' && lowerHost[x] <= 'Z')
      lowerHost[x] = lowerHost[x] - 'A' + 'a';
  }

  if (strncmp(lowerHost, "rtk2go.com", strlen("rtk2go.com")) == 0
      || strncmp(lowerHost, "www.rtk2go.com", strlen("www.rtk2go.com")) == 0
     )
  {
    //Rudamentary user name length check
    if (strlen(settings.ntripClient_CasterUser) == 0)
    {
      systemPrintln("WARNING: RTK2Go requires that you use your email address as the mountpoint user name");
      delay(2000);
    }
  }

  // Set dynamic model
  i2cGNSS.setVal8(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); // Set dynamic model

  clearBuffer(); //Empty buffer of any newline chars
}

//Controls the constellations that are used to generate a fix and logged
void menuConstellations()
{
  while (1)
  {
    systemPrintln();
    systemPrintln("Menu: Constellations");

    for (int x = 0 ; x < MAX_CONSTELLATIONS ; x++)
    {
      systemPrintf("%d) Constellation %s: ", x + 1, settings.ubxConstellations[x].textName);
      if (settings.ubxConstellations[x].enabled == true)
        systemPrint("Enabled");
      else
        systemPrint("Disabled");
      systemPrintln();
    }

    systemPrintln("x) Exit");

    int incoming = getNumber(); //Returns EXIT, TIMEOUT, or long

    if (incoming >= 1 && incoming <= MAX_CONSTELLATIONS)
    {
      incoming--; //Align choice to constallation array of 0 to 5

      settings.ubxConstellations[incoming].enabled ^= 1;

      //3.10.6: To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or both disabled.
      if (incoming == SFE_UBLOX_GNSS_ID_GPS || incoming == 4) //QZSS ID is 5 but array location is 4
      {
        settings.ubxConstellations[SFE_UBLOX_GNSS_ID_GPS].enabled = settings.ubxConstellations[incoming].enabled; //GPS ID is 0 and array location is 0
        settings.ubxConstellations[4].enabled = settings.ubxConstellations[incoming].enabled; //QZSS ID is 5 but array location is 4
      }
    }
    else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
      break;
    else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
      break;
    else
      printUnknown(incoming);
  }

  //Apply current settings to module
  setConstellations(true); //Apply newCfg and sendCfg values to batch

  clearBuffer(); //Empty buffer of any newline chars
}

//Given the number of seconds between desired solution reports, determine measurementRate and navigationRate
//measurementRate > 25 & <= 65535
//navigationRate >= 1 && <= 127
//We give preference to limiting a measurementRate to 30s or below due to reported problems with measRates above 30.
bool setRate(double secondsBetweenSolutions)
{
  uint16_t measRate = 0; //Calculate these locally and then attempt to apply them to ZED at completion
  uint16_t navRate = 0;

  //If we have more than an hour between readings, increase mesaurementRate to near max of 65,535
  if (secondsBetweenSolutions > 3600.0)
  {
    measRate = 65000;
  }

  //If we have more than 30s, but less than 3600s between readings, use 30s measurement rate
  else if (secondsBetweenSolutions > 30.0)
  {
    measRate = 30000;
  }

  //User wants measurements less than 30s (most common), set measRate to match user request
  //This will make navRate = 1.
  else
  {
    measRate = secondsBetweenSolutions * 1000.0;
  }

  navRate = secondsBetweenSolutions * 1000.0 / measRate; //Set navRate to nearest int value
  measRate = secondsBetweenSolutions * 1000.0 / navRate; //Adjust measurement rate to match actual navRate

  //systemPrintf("measurementRate / navRate: %d / %d\r\n", measRate, navRate);

  bool response = true;
  response &= i2cGNSS.newCfgValset();
  response &= i2cGNSS.addCfgValset16(UBLOX_CFG_RATE_MEAS, measRate);
  response &= i2cGNSS.addCfgValset16(UBLOX_CFG_RATE_NAV, navRate);

  //If enabled, adjust GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
  if (settings.ubxMessages[8].msgRate > 0)
  {
    float measurementFrequency = (1000.0 / settings.measurementRate) / settings.navigationRate;
    if (measurementFrequency < 1.0) measurementFrequency = 1.0;

    log_d("Adjusting GSV setting to %f", measurementFrequency);

    setMessageRateByName("UBX_NMEA_GSV", measurementFrequency); //Update GSV setting in file
    response &= i2cGNSS.addCfgValset8(settings.ubxMessages[8].msgConfigKey, settings.ubxMessages[8].msgRate); //Update rate on module
  }
  response &= i2cGNSS.sendCfgValset(); //Closing value - max 4 pairs

  //If we successfully set rates, only then record to settings
  if (response == true)
  {
    settings.measurementRate = measRate;
    settings.navigationRate = navRate;
  }
  else
  {
    systemPrintln("Failed to set measurement and navigation rates");
    return (false);
  }

  return (true);
}

//Print the module type and firmware version
void printZEDInfo()
{
  if (zedModuleType == PLATFORM_F9P)
    systemPrintf("ZED-F9P firmware: %s\r\n", zedFirmwareVersion);
  else if (zedModuleType == PLATFORM_F9R)
    systemPrintf("ZED-F9R firmware: %s\r\n", zedFirmwareVersion);
  else
    systemPrintf("Unknown module with firmware: %s\r\n", zedFirmwareVersion);
}


//Print the NEO firmware version
void printNEOInfo()
{
  if (productVariant == RTK_FACET_LBAND)
    systemPrintf("NEO-D9S firmware: %s\r\n", neoFirmwareVersion);
}
