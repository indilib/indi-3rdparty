# An INDI Roll Off Roof Driver

RollOff ino roof driver using an Arduino to open and close the roof.
The roof driver communicates with an Arduino that in turn interfaces with a roof motor or roof controller. The driver communicates using a basic set of directives. It is the Arduino working from this standard interface with the driver that requires adaptation to control the selected motor. The driver provides an interface sufficient to open, close and determine if the roof is fully opened or fully closed for a roll-off or similar roof.

![Components](components.png)

The driver as with other INDI drivers can be on the same host as the Ekos client or remote on a different computer located in the observatory. The Arduino can be local to the roll-off roof driver using a USB connection. A suitably equired Arduino can be connected to the roll-off roof driver using a WiFi connection. The Arduino activates the circuits that control the roof motor. The motor control can be provided by a re-purposed commercial opener, or a controller suitably built or bought to match requirements for the selected motor.

# The driver

When installed, the RollOff ino driver will be available for selection under Domes in the Ekos profile definition editor.

![Ekos Profile](ekos_profile.png)

Provided as an INDI third party driver. The driver is derived from the Ekos roll-off roof simulator driver. USB is the normal connection method, and it uses a default transmission rate of 38400 baud which can be changed in the Arduino code. The driver responds to Park and Unpark commands from the Ekos Observatory panel. These are sent to the Arduino in the form of open and close requests. The Arduino uses the requests to open or close relays in order to activate roof movement. The driver will generate requests to obtain the state of the fully open and fully closed sensors to determine when the park or unpark request have been completed. The Arduino responds to to those request by reading the assigned switches. There is also an Abort request which should stop any movement in progress. A lock status intended to indicate some form of external mechanical roof lock has been applied. If the lock switch is closed it will block the driver from issuing movement requests. There is an Auxiliary function and status. The use of this auxiliary function is undefined in the driver, if and how it is used is up to the Arduino code.

## Observatory interface.
The observatory interface will be the normal way to park and unpark the roof.

![Observatory](roof_obs.png)


## INDI control panels.
The INDI Control panels provide the detail and setup interfaces.

### The Main Panel
The main panel's periodic monitoring of the roof status as provided by the Arduino code is reflected in the status lights. The Park/Unpark provides local replication of the Observatory functions. The Open/Close buttons might be of use, if for some reason there is a discrepancy between the drivers notion of where the roof is versus the actual position.

![Main Panel](roof_main.png)

### The Connection Panel
The connection panel is where the USB connection baud rate can be set to match the Arduino code. It is where the USB used for the connection is established.

![Connection Panel](roof_connection.png)

### The Options Panel
The options panel is for the setting of the standard driver INDI options and for specifying roof's view of interactions with the telescope mount.

![Options Panel](roof_options.png)

## Weather protection.
The driver will interact with the Ekos weather monitoring applications or DIY local sensors and the watchdog timer along with the other dome related drivers.

# The Arduino
The Arduino code examples include the communication with the driver, the parsing and decoding of the commands and responses. There is communication error checking, any error messages will be sent back to the driver for logging. The mapping of commands to relay activation is provided. Also the mapping of status switch to command status response. The actual hardware pin to relay, and switch to pin assignments are to be specified. The Arduino's communication with the driver is passive, when there is no roof activity it waits for input. The set of built in commands and responses that the driver can handle along with the communication syntax is outlined below in the communication protocol summary.

It is the Arduino code along with hardware build that is responsible for controlling the safe operation of the roof. How that is done is unknown to the driver. The Arduino receives Open and Close commands from the INDI driver. The Arduino will acknowledge receiving the command. The driver will request the status of the fully open and fully closed switches to which the Arduino will respond. The roof must be brought to a stop when it reaches its limits. It must be able to handle obstructions safely. It should be able to respond to Abort requests from the user via the driver. Consideration should be given to what will happen if a switch fails to open or close and how to protect against over running the roof limits.

Using a commercial controller of some kind can simplify the design of the Arduino project. A controller can provide proper sizing of the motor for the size and weight of the roof. Provide obstruction protection, enforce range limits, variable force adjustments, slow start and slow stop to lessen the impact of sudden activation to get the roof moving. Some models will run off solar power and the choice of chain or track. With a controller that can be activated by a single or a pair of on/off button it is a simple job to wire a relay in parallel to emulate the pushing of a button. There is built in support in the example relay code to temporarily close a relay for a particular length of time. Such controllers have their own way of detecting the end of roof travel in order to stop movement. Additional switches are required to notify the Arduino when the roof is fully opened or fully closed. The Arduino can then relay that information to the driver when requested.

## Arduino Examples
Example Arduino code is provided as a starting point. The Arduino code examples provides communication with the driver and template code for reading switches and setting relays. The selected starting code will need to be moved to a directory for development. The name of the directory and the name of the Arduino sketch should be the same. The code name must have a .ino extension. For example ~/Projects/arduino/roof/roof.ino Then you work in the Arduino IDE to edit and load the code onto your Arduino device. The IDE can be downloaded and installed from the arduino.cc web site. You use the IDE to select the type of Arduino board you are working with and define the USB port connecting it. The IDE can be used to edit the code, run builds and load the built sketch onto the Arduino board. https://www.arduino.cc/en/software. Use the most recent Arduino IDE 2 release.

### rolloff.ino.standard.
This is a non specific general example as a starting point. It is close to the AR1450 example. But without the Arduino Due specific code and the pull-down resistor orientation in the specific implementation. If an external controller solution is to be used such as a sliding gate or garage opener controller that provides its own control for stopping the motor when it reaches limits. This might be the Arduino code to use as a starting point. Its default pin assignments match the arduino.cc relay shield. Relay 1 to 4 being activated using pins 4, 7, 8, 12. If using a single button controller just relay 1 would need connection wiring. The default pins for the input switches is A0 through A3. The fully open switch connects to pin A0 and the fully closed switch is connected to pin A1.

### rolloff.ino.ar1450.
This is a specific implementation that used an Arduino Due. It works with an Aleko AR1450 sliding gate opener controller and motor to activate the roof. A single relay is used with this motor controller to work in place of a a single push button interface. The motor direction alternates with each use. Magnetic sensors provided with the Aleko controller determine when the roof movement stops. If a blockage is detected the contoller will either reverse direction or stop depending upon if it was in the process of opening or closing. A remote FOB can still be used but its movement of the roof is independent of the roof driver. The Aleko AR1450 model does not provide soft start/stop.

### rolloff.ino.boutons.
This user provided example evolved from Arduino Uno code that operated using local buttons and it is able to directly control the motor. The driver communication was added to operate in parallel with the existing code to provide remote capability. It uses a bank of relays to switch polarity to the motor to control direction. The auxiliary function from the driver is used to remotely turn on power for the motor. This Arduino code maintains the state of the roof and monitors local switches to determine when to stop the motor. This approach would be of interest if planning to design that takes direct control of a motor.

### rolloff.ino.linear_actuator.
User provided example that operates linear actuators to open and close the roof.
This version of the rolloff.ino has been modified to work with linear actuators that turn
themselves off when they reach their full extension and require the power to be 'inverted'
(+tive to -tive and -tive to +tive) so they can be retracted.

### rolloff.ino.wifi.
This is some test code for using WiFi to connect to the driver. It requires an Arduino model that supports the WIFININA library such as the Uno WiFi Rev2. The code is the rolloff.ino.standard modified to use WiFi instead of USB. Might be useful if the observatory computer is located on or next to the telescope. As coded it works on WiFi networks using WPA2 for security. If using an open or WEP secured network changes as outlined in the Arduino WIFININA documentation will be needed. It uses a permanently defined internet address and port that matches the definition in the rolloffino driver. The changes are identified by the conditional #USE_WIFI. If it was wanted to use WiFi with one of the other examples similar edits could be made. As with the rolloff.ino.motor it is example code without user feedback. 

### rolloff.ino.motor.
This is some test code to control a 12 volt motor using a SyRen motor driver. This Uno example is one way to work with a motor rather than via a controller such as a gate opener or garage door opener. The fully open and fully closed switches are used to know when to stop the motor. The code uses a SyRen motor driver to vary power to the motor and reverse polarity in order to change direction. It uses soft start and soft stop by ramping up and down the speed of the motor to open and close the roof. The SyRen powers the motor and can also supply 5 volts to power an Arduino. It includes code for using local buttons. It has just been bench tested without a load. The SyRen documentation indicates a 12V battery should be employed to deal with the power surges. SyRen also have a 50A model in addition to the 25A one shown in the image. Care needs to be taken matching the power of the moter to the installation requirements. If a gear motor is used it should able to disconnect a clutch so if needed the roof could be manually moved. Matching the motor spindle to the drive mechanism can also be a challenge. If using sensors to slow or stop a motor, interrupt routines may provide more precision than polling.

### rolloff.ino.stub.
A sketch that could be used for initial testing of communication between the driver and an Arduino.
When loaded it operates without any switch or relay connections. It simply accepts initial connect, and open/close commands from the driver. After a delay it returns fully open or fully closed status to the driver. 

# Arduino Code Overview

Overview of the code used in the rolloffino.standard example. This is to assist those new to working with Arduino code and needing to customize. If using a controller that can be operated via relays and you use the default Arduino pins to connect the relay(s), there might not be any need for modifications.

## Definitions section
Modify to requirements

The terms HIGH, LOW, ON, OFF are defined by the Arduino environment and explained in the Arduino IDE documentation.

For the relays and switches in use define the pin numbers that match the wiring. The commands from the driver OPEN, CLOSE, ABORT, AUXSET are associated with a relay RELAY_1, RELAY_2, RELAY_3, RELAY_4. And those relay names are in turn defined to be a particular pin number on the Arduino. Define similar associations for the driver requests to read the fully open, fully closed and other switches.

Then you define for each of the relays if they are to be set and left in the requested state or if just a momentary set is to be used. If for the relay associated with the command the HOLD is set to 0, it will be closed and released. The timing of this is defined below by the DELAY settings. The idea is to emulate a push and release of a button. If for particular a relay HOLD is defined as 1, then the relay will be set and left closed or opened depending upon the request.

Following the definitions for the switches and relays are the internal buffer definitions and limits which should be left alone, they are matched in the driver.

Next any additional error messages can added for logging by the driver.

## Utility routines

### sendAck and sendNak
Used to respond to each driver input and used as is. Each input must receive a positive or negative response. A positive response will return the result. A negative response will be accompanied by an error message.

The next two routines might require modification depending upon how you connect your switches and relays.

### setRelay
Modify to requirements
Requires modification depending upon how you connect your relays. The comments in the routine header might relate if you use a module that provides a bank of relays and associated circuitry. It describes how to connect an Arduino and have it isolated and protected from back surges from the relay. There is help on this wiring available on-line. Use the comments in the body as a guide for when the relay should be opening or closing. Determine given your relay module and wiring how it should be controlled and if needed change around the use of HIGH and LOW in the routine.

### getSwitch
Modify to requirements
Requires modification depending upon how you connect your switches. Similar to the setRelay is the question of how the switches are setup and if they have pull-up or pull-down resistors. They must have one or the other. If unsure read about it in the Arduino IDE documentation. In summary if external pull down resistors are used an open switch will return LOW. If external or the Arduino's internal pull-up resistors are used an open switch will return HIGH. So whether open switch is HIGH or LOW match that to the OFF in the routine.

## Communication routines

### parseCommand
Used by the readUSB routines to break down the strings from the driver into their parts.
readUSB

### readUSB
Determines type of input. Resolves the named action or state into an associated relay or switch and its associated pin number. If it is an initial connection request it willl acknowledge that with the local code's version number. Returns a negative acknowledgement with message for any errors found. When valid command or request is decoded it calls commandReceived or requestReceived routines. These routines are meant to provide for additional action if something other or in addition to the default behavior is wanted. It also calls out to isAbortAllowed routine in case non default decision making is wanted. If using a commercial opener of some kind where needed action can be related to the use of a relay the default actions may well suffice. If you need to locally track the motor movement then these breakout points might be of interest.

## Intervention routines
These routines are when the final action for a command or request is about to take place. They are brought out from readUSB to be more obvious. They would be extended or changed if the default use of a relay was not what was needed. The rolloff.ino.motor sketch demonstrates an example of extending the functionality. It defaults to the relay for some commands and uses additional routines for others such as open, close and stop.

### isAbortAllowed
Change if default conditionals are not what you need.
The readUSB routine has received an Abort command. The default implementation is working with just the one button to control the roof. The decision whether or not to proceed is taken in this routine. Returning false will send negative acknowledgement to the driver. Returning true will result in the abort command being allowed through to the commandReceived routine which will activate the the associated relay.

### commandReceived
Change if default action of setting a relay is not what you need.

### requestReceived
Change if default action of reading the pin and sending result to the driver is not what you need.

## Standard routines
### setup
Modify to requirements
If using an AtMega variant Arduino model you can elect to use the built in pull-up resistors on the input pins. To do this use INPUT_PULLUP instead of INPUT definition for the input pins. For the setting of the relay pins use HIGH or LOW depending on which sets your relays to open.

### loop
This is the Arduino loop that waits for driver input.

# Communication Protocol
Outline of the communication between the roof driver and the Arduino
The communication uses a simple text protocol, lines of text are sent and received.
A Command is from the rolloffino driver to the Arduino sketch. A Response is from the Arduino sketch to the rolloffino driver. The driver will log these commands and responses to a file when debug logging is enabled.

```
Command Format:  (command:target|state:value)
Response Format: (response:target|state:value)

Command:   CON 	(CON:0:0)              Establish initial connection with Arduino
	   GET	(GET:state:value)      Get state of a switch
	   SET 	(SET:target:value)     Set relay closed or open

           state:   OPENED | CLOSED | LOCKED | AUXSTATE
           target:  OPEN | CLOSE | ABORT | LOCK | AUXSET
           value:   ON | OFF | 0 | text-message

Response:  ACK       Success returned from Arduino
	   NAK       Failure returned from Arduino

Examples:        From the Driver      Response From the Arduino
                 ----------------     --------------------------
Initial connect (CON:0:0)          >   	
                                   <  (ACK:0:version) | (NAK:ERROR:message)
Read a switch   (GET:OPENED:0)     >
                                   <  (ACK:OPENED:ON|OFF) | (NAK:ERROR:message)
Set a relay     (SET:CLOSE:ON|OFF) > 
                                   <  (ACK:CLOSE:ON|OFF) | (NAK:ERROR:message)
```

# A project build example.

The images match with the rolloffino.ino.ar1450 install.

The pin connections. The baloon looking things next to the pull-down resistors are meant to be the fully open/closed switches. With the pull-down resistors an open switch would return LOW when the pin is read. The connection to the Aleko controller to act in place of the single push button comes off of the Normally Open relay NO connection.

![Due Wiring](layout_due.jpg)


The Arduino Due with a relay board. The additional DC supply is to provide the higher voltage required for the relays. The relay bank is opto-isolated from the protected 3.3V Arduino Due which has its own supply. Here only one of the two USB connections is in use. The other can be connected if debug logging is required. Just the single relay on the right is in use with the red and black wires routed to the Aleko controller. When the relay closes, then opens it will replace the action of a manual push button closing a connection.

![Due Image](arduino_due.jpg)


An Arduino Uno with a relay shield would make for a cleaner install here. With its 5 volt power and built in pull-up resistors it can be a more straight forward installation.

![Uno Shield](uno_shield.png)


The Aleko controller board. The AC power supply comes in at the top. To the lower left is the red and black wires feeding back to the Arduino relay. An adjustable potentiometer is visible around the midlle to control how much power is applied to overcome resistance before a blockage is declared.

![Aleko Controller](aleko_controller.jpg)

Just an image showing the Aleko positioning magnet that the controller detects when the roof has reached the fully opened or fully closed position. It shows the track used to drive the roof. The roof will roll a little past where the detector is placed.

![Aleko Magnet](aleko_magnet.jpg)

Example of a fully closed detection switch feeding to an Arduino pin.

![Motor Wiring](switch_closed.jpg)

From Arduino rolloff.ino.motor, wiring for controlling a DC motor using a SyRen driver. The fused power distribution box supplies 12V to the SyRen which in turn feeds 12V to the motor. The switches are used for the open and closed switches. Uno pin 11 white wire is controlling the SyRen. The black and red wires from the SyRen provides ground and 5 volts to power the Uno.
In addition to the 25A model shown here, there is also a SyRen 50A. SyRen recommends the use of a battery with the source power to cope with the surges. When building from components care will be needed to matching the motor power and torque to the roof requirements. If a gear motor is used, there should be a way to disengage a clutch to allow the roof to be moved manually if needed. Matching the spindle to the drive mechanics might also be a challenge. 

![Motor Control](motor.jpg)


