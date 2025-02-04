# Configure with Serial

Surveyor: ![Feature Supported](img/GreenDot.png) / Express: ![Feature Supported](img/GreenDot.png) / Express Plus: ![Feature Supported](img/GreenDot.png) / Facet: ![Feature Supported](img/GreenDot.png) / Facet L-Band: ![Feature Supported](img/GreenDot.png)

**Note:** Starting with v3.0 of the firmware any serial menu that is shown can also be accessed over Bluetooth. This makes any configuration of a device much easier in the field. Please see [Configure With Bluetooth](docs\configure_with_bluetooth.md) for more information.

To configure an RTK device using serial attach a [USB C cable](https://www.sparkfun.com/products/15425) to the device. The device can be on or off.

## RTK Surveyor / Express / Express+

[![RTK Surveyor Connectors and label](https://cdn.sparkfun.com/assets/learn_tutorials/1/4/6/3/SparkFun_RTK_Surveyor_-_Connectors1.jpg)](https://cdn.sparkfun.com/assets/learn_tutorials/1/4/6/3/SparkFun_RTK_Surveyor_-_Connectors1.jpg)

*The SparkFun RTK Surveyor has a variety of connectors*

Connect the USB cable to the connector labeled **Config ESP32**.

Once connected a COM port will enumerate. Open the `Device Manager` in Windows and look under the Ports branch to see what COM port the device is assigned to.

## RTK Facet

[![RTK Facet USB C Connector](https://cdn.sparkfun.com/r/600-600/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_Facet_-_Ports_-_USB.jpg)](https://cdn.sparkfun.com/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_Facet_-_Ports_-_USB.jpg)

Connect the USB cable to the USB connector.

There is a USB hub built into the RTK Facet. When you attach the device to your computer it will enumerate two COM ports.

[![Two COM ports from one USB device](https://cdn.sparkfun.com/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_Facet_-_Multiple_COM_Ports.jpg)](https://cdn.sparkfun.com/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_Facet_-_Multiple_COM_Ports.jpg)

In the image above, the `USB Serial Device` is the ZED-F9P and the `USB-SERIAL CH340` is the ESP32.

**Don't See 'USB-Serial CH340'?** If you've never connected a CH340 device to your computer before, you may need to install drivers for the USB-to-serial converter. Check out our section on <a href="https://learn.sparkfun.com/tutorials/sparkfun-serial-basic-ch340c-hookup-guide#drivers-if-you-need-them">"How to Install CH340 Drivers"</a> for help with the installation.

**Don't See 'USB Serial Device'?** The first time a u-blox module is connected to a computer you may need to adjust the COM driver. Check out our section on <a href="https://learn.sparkfun.com/tutorials/getting-started-with-u-center-for-u-blox#install-drivers">"How to Install u-blox Drivers"</a> for help with the installation.

Configuring the RTK device is done over the *USB-Serial CH340* COM port via the serial text menu. Various debug messages are printed to this port at 115200bps and a serial menu can be opened to configure advanced settings. 

Configuring the ZED-F9P is done over the *USB Serial Device* port using [u-center](https://learn.sparkfun.com/tutorials/getting-started-with-u-center-for-u-blox/all). It’s not necessary for normal operation but is handy for tailoring the receiver to specific applications. As an added perk, the ZED-F9P can be detected automatically by some mobile phones and tablets. If desired, the receiver can be directly connected to a compatible phone or tablet removing the need for a Bluetooth connection.

## Terminal Window

Open a terminal window at 115200bps; you should see various status messages every second. Press any key to open the configuration menu. Not sure how to use a terminal? Check out our [Serial](https://learn.sparkfun.com/tutorials/terminal-basics) Terminal Basics](https://learn.sparkfun.com/tutorials/terminal-basics) tutorial. 
Note that some Windows terminal programs (e.g. Tera Term) may reboot the Facet when the terminal connection is closed. You can disconnect the USB cable first to prevent this from happening.


[![Terminal showing menu](https://cdn.sparkfun.com/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_ExpressPlus_MainMenu.jpg)](https://cdn.sparkfun.com/assets/learn_tutorials/2/1/8/8/SparkFun_RTK_ExpressPlus_MainMenu.jpg)

*Main Menu*

Pressing any button will display the Main menu. The Main menu will display the current firmware version and the Bluetooth broadcast name. Note: When powered on, the RTK device will broadcast itself as either *[Platform] Rover-XXXX* or *[Platform] Base-XXXX* depending on which state it is in. The Platform is 'Facet', 'Express', 'Surveyor', etc.

Pressing '1' or 's' for example, will open those submenus.

The menus will timeout after 10 minutes of inactivity, so if you do not press a key the device will exit the menu and return to reporting status messages.

![Configuration menu open over Bluetooth](img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Exit%20BEM.png)

*Configuration menu via Bluetooth*

**Note:** Starting with firmware v3.0, Bluetooth-based configuration is supported. Please see [Configure With Bluetooth](docs\configure_with_bluetooth.md) for more information.

