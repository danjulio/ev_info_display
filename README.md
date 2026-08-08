## EV Info Display
Firmware for an EV performance information gauge using the [Waveshare ESP32-S3 2.8" touchscreen](https://www.waveshare.com/product/arduino/displays/lcd-rgb/esp32-s3-touch-lcd-2.8c.htm) development board.  It connects via the vehicle's OBD2 port.

![EV Info Display](pictures/ev_info_display.jpg)

EV Info Display can show several screens showing information not typically displayed on most EV dashboards but interesting none-the-less.  Note that not all data is displayed for various supported EVs (it's based on what the vehicle can provide).

1. Speed/Torque/Elevation
2. Power consumption/regeneration (kW) and Auxiliary systems power consumption
3. Traction and low-voltage battery voltage, temperature and current
4. Individual traction battery cell voltages

In addition it contains a speed test screen allowing timing of the vehicle 0-60 MPH (0-100 KPH) performance.

Connection to the vehicle may be made via several methods.

1. Direct CAN Bus connection (using an external CAN Bus transceiver chip such as the TJA1057)
2. Wifi based ELM327 OBD2 module
3. Bluetooth low-energy (BLE) based ELM327 OBD2 module (traditional Bluetooth serial is not supported)

EV Info Display has been tested with the following ELM327 OBD2 modules.

1. LELink2 OBD-II BLE module
2. MeatPi Electronics WiCAN-PRO (Wifi only)

It should also work with the Vgate iCar Pro 4.0 BLE dongle but this is untested at the moment.

Currently supported vehicles include

1. Nissan Leaf ZE0 (2013-2017 models) experimental support
2. Nissan Leaf ZE1 (2018-2025 models)
3. VW MEB RWD platform with 84-cell traction battery (e.g. ID.3 Pure)
4. VW MEB AWD/RWD platform with 96-cell traction battery (e.g ID.3, ID.4, European ID.Buzz)
5. VW MEB AWD/RWD platform with 104-cell traction battery (e.g. U.S. ID.Buzz)
6. VW MEB RWD platform with 108-cell traction battery (e.g. ID.3 Pro)


The firmware is designed to allow easy addition of new vehicles.  A set of instructions may be found below.  You'll need the OBD2/CAN UDS bus transactions necessary to get the various datums and a vehicle to test in.

Also look below to find instructions for easily loading pre-compiled firmware binaries onto the Waveshare board.

### Operation

EV Info Display will attempt to connect to the selected interface when powered up.  Swipe left or right between screens.  The selected screen is stored to persistent storage after 15 seconds.

#### Speed/Torque/Elevation
![Screen 1: Speed/Torque/Elevation](pictures/gui_tile_torque.jpg)

There are two arcs for AWD vehicles (outside arc is rear drive, inside arc is front drive).  Elevation is based on vehicle GPS receiver and may be off by many meters (GPS elevation is less accurate).

#### Power Consumption
![Screen 2: Power](pictures/gui_tile_power.jpg)

Primary display shows traction power or regeneration.  Traction power is a positive number and regeneration (or charging) is a negative number.

Auxiliary power consumption is vehicle-specific but usually contains low-voltage (12V) system consumption and high-voltage items such as heating/cooling and battery warming.  Depending on the vehicle, auxiliary high-voltage consumers may also be reflected in the primary consumption display.

#### Electrical Information
![Screen 3: Electrical](pictures/gui_tile_electrical.jpg)

Primary display shows traction battery current with the arc display and digital readout and battery voltage and min/max pack temperatures.  Traction current is displayed as a positive number and regeneration/charging current as a negative number.

The secondary display shows the low voltage battery voltage with the arc display and digital readout and current (charge current is positive, discharge current is negative) as well as temperature if it is available.

#### Traction Battery Cell Voltages
![Screen 4: Cell Voltages](pictures/gui_tile_cells.jpg)

Displays a bar graph of individual traction battery cell voltages.  The minimum and maximum cell voltage and cell number are displayed above and the delta voltage below.  The graph is scaled between 3.0 and 4.2 volts to quickly get a sense of pack condition and to visualize voltage sags in cells under load.

The minimum voltage cell bar is red and the maximum voltage cell bar is yellow.  All other cell bars are blue.

Note: On some platforms like VW MEB, each cell voltage is read using a separate OBD request making updates fairly slow when using an ELM327 interface.

#### Speed Test
![Screen 5: Speed Test](pictures/gui_tile_timed.jpg)

This screen allows you to time the vehicle's 0-60 MPH (0-100 KPH) performance with a fun race-track style display.  The vehicle must be stopped to initiate the test.  Pressing the ```START``` button starts the countdown timer with one LED blinking per second and set of tones.  Try to start your run just as the green LED flashes although the timer actually starts when the first non-zero speed is read.  Time timer stops when the vehicle reaches 60 MPH (100 KPH).  Jump starts or uncompleted runs result in a red LED flashing at the end.

#### Settings Screen
![Screen 6: Setup](pictures/settings_screen.jpg)

The settings screen configures display operation.

1. Connection Status is displayed when a connection has been made to the vehicle.
2. Vehicle pull-down allows selection of vehicle type.
3. Interface allows selection of the interface type.  Selecting BLE or Wifi will bring up an additional settings screen.
4. Brightness changes the backlight intensity.
5. The metric switch changes units (MPH and °F or KPH and °C).
6. Firmware version displays the current firmware revision level.

Press the Save button to commit a set of changes.  The display will reboot with the new settings in place.  Swiping away from the settings screen will cause any changes except brightness to be lost (although the new brightness level will not be saved).  This allows temporary changes of brightness without having to reboot (e.g. going from day to night).

#### BLE Setup
![BLE setup screen](pictures/ble_settings_screen1.jpg)

EV Info display will attempt to automatically connect to any BLE module it knows about when BLE is the selected interface.  However, the BLE setup screen allows entering a custom set of 16-bit UUIDs for any unsupported module.  Switching the Enable Custom switch on displays the Service, TX (EV Info Display to interface) and RX (interface to EV Info Display) Characteristic UUIDs. 

![BLE setup screen with enabled UUIDs](pictures/ble_settings_screen2.jpg)

Touching a UUID displays an edit screen with keyboard.

![BLE Service UUID screen](pictures/settings_ble_uuid_keyboard.jpg)

Click ```X``` to cancel, ```√``` to set the UUID.  Then in the BLE Setup window click ```SAVE``` to store the configuration in persistent memory, CANCEL to return to the settings screen with no changes.

#### Wifi Setup
![Wifi setup screen](pictures/wifi_settings_screen.jpg)

EV Info Display connects to the interface module's Wifi Access Point using a socket connection to the device's ELM327 port.  This screen configures the module SSID, optional password (leave the password blank for no password) and port number.  Clicking on any field brings up an edit screen with keyboard.

![Wifi Text entry screen](pictures/settings_wifi_ssid_keyboard.jpg)

![Wifi Numeric entry screen](pictures/settings_wifi_port_keyboard.jpg)

Click ```X``` to cancel, ```√``` to set the entry.  Then in the Wifi Setup window click ```SAVE``` to store the configuration in persistent memory, CANCEL to return to the settings screen with no changes.

#### Known Issues

1. Wifi and BLE connections are significantly slower than a direct CAN connection.  This is due to the overhead of managing the ELM327 interface when the vehicle uses different CAN IDs for each datum that is read.  Performance varies by vehicle type.  For example the VW MEB platform is much slower than the Nissan Leaf ZE1 platform.  The Leaf only uses a few CAN IDs while the VW MEB platform uses many more requiring a lot more communication overhead.  EV Info Display attempts to optimize configuring the ELM327 interface, only changing the necessary configuration to minimize overhead.  Best performance is always obtained using a direct CAN interface because there is no overhead.
2. I have seen, on rare occasion, the VW MEM platform stop replying to particular query items resulting in a frozen value for that item.  Powering off the car and turning it back on fixes the problem.
3. There is a bug in the Espressif IDF when using BLE.  EV Info Display will attempt to repeatedly poll for BLE devices to connect too.  However randomly the polling process will crash the ESP32 and reboot the device.  I tried to debug this but I think it's deep in some "blob" code.  During normal operation it shouldn't pose much of a problem as EV Info Display will immediately connect to the BLE interface module.
4. The Cell Voltage graph glitches slightly when swiping.  I think this is an issue of PSRAM access (where the drawing canvas is stored) and the LCD Update peripheral.
5. Nissan Leaf AZEO HW CAN interface is slow because the ESP32 TWAI driver seems to miss responses among all the raw car traffic it sees on the CAR CAN interface that's present on the ODB Port.  This results in slow/jerky updates.  For the AZE0 it can be better to use a BLE or Wifi dongle.

### Installation
Please see below for information about a 3D printed enclosure that can be used to hold the Waveshare board.

#### USB Power
The simplest installation occurs when using the BLE or Wifi interfaces.  Just plug the Waveshare board into a vehicle USB socket.  The vehicle will manage turning EV Info Display on and off.

You may need to remove the OBD2 module when leaving the car off for long periods of time as the OBD2 port is always powered (some modules like the WiCAN Pro can be configured to power down when the vehicle is turned off).

#### USB Power with CAN Interface breakout
Using the direct CAN bus interface requires adding a CAN Bus transceiver chip to interface the Waveshare board's ESP32-S3 internal CAN Bus interface to the CAN bus signals going to the OBD2 port.  The transceiver must be capable of operating with 3.3V logic levels for its TX and RX pins.  I found this [CAN Transceiver breakout board](https://www.amazon.com/dp/B0DXDR1ZJ6) on Amazon that works.  I also used this [OBD2 male connector](https://www.amazon.com/dp/B083FFTJGN) but there are many that will work.

1. Connect USB Power to the Native USB connector (not the Serial USB connector)
2. Connect the CAN Transceiver breakout board as shown below.  You can use the wiring harnesses that come with the Waveshare board.  Note that you'll have to create a twisted pair wire harness to the OBD2 connector.
3. The CAN Transceiver breakout should not have a termination resistor.

![Wiring with CAN breakout board](pictures/can_breakout_wiring.jpg)

![Wiring diagram for CAN breakout board](pictures/can_transceiver_wiring_diagram.jpg)

![OBD2 connector pinout](pictures/obd2_pinout.jpg)

#### OBD2 Power with Switched CAN Interface board
I designed a custom open-source PCB called [Switched CAN Interface](https://github.com/danjulio/switched_can_interface) to support devices like EV Info Display.  It provides a 5V power supply that can automatically turn on when the OBD2 port voltage rises above about 13.2V (vehicle DC-DC switched on) and off when the voltage falls below 13V (vehicle DC-DC switched off).  It also includes a CAN Bus transceiver.  It is connected as shown below.

![Wiring with Switched CAN Interface](pictures/switched_can_if_wiring.jpg)

![Wiring diagram for Switched CAN Interface](pictures/switched_can_if_wiring_diagram.jpg)

Note that the automatic switching does not work well with the Leaf because of the way the Leaf manages charging its 12V battery.  The Leaf generally quickly drops the charge voltage to about 13V after starting which will switch EV Info Display off.  Fortunately the Leaf provides a switched 12V power on the OBD connector and the Switched CAN Interface can be configured to use it as shown below.

![Wiring diagram for Switched CAN Interface on Leaf](pictures/switched_can_if_wiring_leaf.jpg)

### Building the firmware
EV Info Display is built using the [Espressif IDF ](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/index.html) version 5.5.5.

Assuming you have [installed](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/get-started/index.html) the IDF and a local copy of this repo on your computer, change to the ```firmware``` directory and perform the following steps.

1. Execute the IDF configuration script
2. Type ```idf.py build```

The firmware binaries will be put in a created ```build``` directory.  The following binaries are programmed into the Waveshare board via one of its USB ports.

| Binary | Location | ESP32-S3 memory location to load |
|---|---|---|
| bootloader.bin | build/bootloader | 0x0000 |
| partition-table.bin | build/partition_table | 0x8000 |
| ev\_info_display.bin | build | 0x10000 |

The firmware may be loaded using the command

	idf.py -p [SERIAL_PORT] -b 921600 flash
	
where ```[SERIAL_PORT]``` is the device or device file for the serial port associated with the Waveshare board (e.g. a ```COM``` port on Windows and something like ```/dev/cu.usbmodem1101``` on Mac).

#### Log information
The firmware logs various events to the native USB Serial port.

#### Releases

| Version | Description |
|---|---|
| 0.1 | Initial Release |
| 0.2 | MEB Reverse bug fix, smoother display and minor cleanup<br>1. Add slight averaging for data when using TWAI (direct CAN) IF for smoother numeric displays (less jittery).<br> 2. Reduce number of averaged timestamps to compute largee arc change delta time intervals for smoother updates.<br> 3. Stop any ongoing arc animations on each update to prevent occasional apparent jumps in meter position.<br> 4. Detect MEB platform gear position to correctly display positive torque during reverse.<br> 5. Add untested support for Vgate iCan Pro 4.0 BLE dongle.<br> 6.  Misc code cleanup. |
| 0.3 | 1. Added Traction Battery Cell Voltage tile.<br>2. Fixed a bug in the data smoothing when an ELM327 interface is selected.<br>3. Added vehicle support for indexed data items.<br>4. Moved to LVGL v8.3.11 (from v8.2).<br>5. Fixed a bug where tile would be shown even if vehicle didn’t support its items.<br>6. Increased task stack size. |
| 0.4 | 1. Added experimental Nissan Leaf AZE0 support.<br>2. Added additional VW MEB platforms (84, 104, 108 cell-count batteries).<br>3. Increased display contrast for better visibility.<br>4. Adjusted displayed gauge limits to better match actual vehicles.<br>5. Support for LE Link devices that return an incorrect ELM327 version string.<br>6. Work-around for VW MEB platform bug in reporting GPS elevation over 3274 meters.<br>7. Fix race condition in MEB code that might result in a load exception and reboot.<br>8. Internal changes for determining gauge tick counts and added ability to reset CAN interface faster on timeouts (for AZE0).<br>9. Modified code to support IDF v5.5.5 compiler checks. |

### Programming pre-compiled firmware
The built binaries may be found in the ```precompiled``` directory in this repo.  There are a variety of ways to load these into the Waveshare board, including using the IDF, Espressif's Windows-only utility program or their web-browser based serial flasher described here.

The [web-browser based flasher](https://espressif.github.io/esptool-js/) requires a browser that supports hardware serial connections so Safari won't work.

Plug the waveshare board into your computer and identify what serial port it is using.  Using the UART USB serial port is safest as it will work when the flash has been erased.  Then make sure you have the three binary files loaded on your computer.

1. Press ```Connect``` under the Program header.
2. Select the appropriate serial port and click ```Connect``` on the pop-up.
3. Add the three binary files for programming at the specified locations as shown below (you'll have to ```Add file``` twice.
4. Click ```Program``` to flash the files.

Upon a successful flash you should see something like the following.

![Flasher web page](pictures/flasher_web_page.jpg)

When done press the Reset button on the Waveshare board to start running the code and display the splash screen.

![EV Info Display boot screen](pictures/splash_screen.jpg)

### Firmware Architecture

![Firmware Architecture Block Diagram](pictures/fw_arch.jpg)

The firmware consists of two FreeRTOS tasks, each running on a separate core.  [LVGL](https://lvgl.io/) is used to provide the graphical user interface.

```can_task()``` running on the Protocol (PRO) core manages communication over the OBD port, requesting various datums and providing that information to ```gui_task()``` running on the Application (APP) core via a thread-safe Data Broker.

The CAN Task consists of two components.  The Vehicle Manager evaluates logic in the selected vehicle module to generate OBD CAN message requests, one at a time.  This logic is responsible for vehicle-specific actions, such as requesting HV voltage and current and then returning the Power datum to the GUI for display.  The CAN Manager forwards the OBD CAN message requests through the selected driver, which may convert its form as necessary (for example into ELM327 serial stream commands), and then takes the response and provides that back to the vehicle module for processing.  The TWAI driver sends the request directly through the ESP32's TWAI interface.  The ELM327 converts the request into a series of one or more ELM327 serial stream data packets.  These packets are sent through the selected BLE or Wifi stacks.  The ELM327 driver attempts to minimize the number of serial stream data packets sent to the ELM327 controller to improve performance since only one packet may be sent at a time and the driver must wait for an acknowledgement.

The GUI Task initializes LVGL and then acts as an event broker for various incoming data and internal LVGL events.  The GUI is organized as a series of screens for the various functions of the device.  The Main Screen contains a series of full-screen tiles for each of the main displays that can be swiped between.  Each Main Screen tile registers a set of datums that it wants through the Data Broker with the Vehicle Manager.  The registration contains the datum and a callback routine to receive the data (which is atomically synchronized by the Data Broker).

A Persistent Storage module, implemented using the ESP32's NVRAM flash abstraction, configures both tasks.  It may be updated by the GUI Task.  In order to simplify the software architecture, other than backlight brightness, all changes to persistent storage are only applied at power-on, necessitating a reset after changes are made.

#### Adding a new vehicle type

There is no standardization of OBD commands in the BEV world so each different vehicle type requires its own vehicle specific module consisting of a ```.c``` and ```.h``` file that are included in the ```vehicle``` sub-directory and used in the```vehicle_manager.c``` file.  This module contains a list of OBD commands to send for the various items necessary to get all supported datums.  It also contains a set of logic that is periodically evaluated by the Vehicle Manager which sends one command at a time and handles the responses from the CAN Manager.  This logic may be configured for the specific set of datums the currently displayed tile requires.  It is responsible for taking vehicle-specific pieces of data and creating the datums that the firmware defines in ```data_broker.h```.

1. Create vehicle-specific ```.c``` and ```.h``` files in ```vehicle```.  Use existing files as a template.
2. Create ```const vehicle_config_t``` data structures to define specific vehicles (supported datums, datum ranges and pointers to internal functions.
3. Create a list of ```can_request_t``` data structure to define the UDS commands for each supported datum.  The datums are defined in ```data_broker.h```.
4. Define any necessary constants and private data needed to convert the request data to displayed datums.  For example I use knowledge of the vehicle's current "gear" to negate the MEB torque value in reverse so the display always shows positive torque for movement and negative torque values for regenerative braking.
5. Implement a set of functions required by the ```vehicle_config_t``` data structure to initialize, evaluate and handle call-backs for the new vehicle.
6. Add the vehicle header file to ```vehicle_manager.c``` and update the ```static const vehicle_config_t* vehicle_listP``` data structure and ```NUM_VEHICLES``` constant with the new vehicle.

Note that the ```vehicle_config_t``` ```name``` member will be displayed in the GUI Settings Vehicle drop-down (and stored in persistent storage).

Five functions must be defined in step 5 above and are described below.  Vehicle implementations may of course include other internal functions.

1. ```fcn_init``` is called after boot before evaluation starts.  Typically it is used to enable CAN filtering for cars like the Leaf AZE0 that don't have a CAN gateway and present all internal traffic on the OBD port or disable CAN filtering for cars that have a CAN gateway module using the ```can_en_rsp_filter``` function.
2. ```fcn_eval``` is called every 10 mSec by ```can_task``` through ```can_manager```.  It is responsible for loading new UDS requests to update datums.  It should only allow one outstanding request at a time.  Requests are satisfied by the receipt of data or an error.  Error cases include things like timeout and negative CAN responses from the car.
3. ```fcn_set_req_mask``` is called by ```vehicle_manager``` on behalf of the ```gui_task``` with a bitmask containing ```data_broker``` datums to request for the currently displayed screen.  It configures the list of UDS requests to make to satisfy the screen without constantly requesting items that will not be displayed.
4. ```fcn_rx_data``` is called by ```vehicle_manager``` with the UDS response ID, length and data for the currently outstanding request.  The ID should be mapped back to the datum, data converted to the datum's desired format and ```vm_update_data_item``` or ```vm_update_indexed_data_item``` called to get the data to the GUI.
5. ```fcn_note_can_error``` is called by ```vehicle_manager``` with errors. Currently only a request timeout is reported this way (immediate errors are reported when the request is made with ```can_tx_packet``` is called in the ```fcn_eval``` routine and errors like a negative response are reported as timeouts).

The ```vehicle_leaf_ze1``` files are a good example of a simple vehicle implementation.  The ```vehicle_vw_meb``` files are more complex and include some work-arounds for issues found in that platform's UDS interface.

### Enclosures
See the ```enclosure``` directory for CAD files for a simple 3D-printed enclosure.  It can be printed in two forms.

1. Short enclosure holding only the Waveshare board.
2. Taller enclosure holding the Waveshare and Switched CAN Interface (or other CAN transceiver) boards.

#### BLE/Wifi interface enclosure
The Waveshare board in the short enclosure base with the native USB port exposed.

![Short enclosure assembly](pictures/short_enclosure_assembly.jpg)

#### Switched CAN Interface board enclosure
Switched CAN Interface board mounted component-side down and the CAN+power wires run under the board, exiting via the hole in the base.  Make sure to route the wires as shown so as not to interfere with the Waveshare board sitting above.

![Tall enclosure assembly](pictures/tall_enclosure_assembly.jpg)

#### Assembly screws
The screws for my builds came out the screw jar but hopefully the following picture is helpful.  

![Screws I used](pictures/assembly_screws.jpg)

I drilled small holes through the enclosure base tab and the top piece to allow a very thin screw to hold these pieces together.

![Top screw mounting](pictures/top_screw_mounting.jpg)
