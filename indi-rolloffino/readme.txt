INDI rolloff roof driver to communicate with an Arduino controller.
-------------------------------------------------------------------

Intended as a basis for a DIY home build project that uses a Arduino controller for roof movement.
Provided in a form similar to a INDI third party driver along with some Arduino code examples. The driver is
derived from the rolloff roof simulator. USB is the connection mechanism, it uses a default transmission rate of
38400 baud defined in the Arduino code.

The Arduino code is responsible for controlling the safe starting and stopping of roof movement. The Arduino receives
Open and Close commands from the INDI driver. The driver expects in return a fully open or fully closed response.
There are three examples of Arduino code matching the protocol used by the driver.

1. rolloff.ino.standard. This is a non specific general example as a starting point. It has not been tested.

2. rolloff.ino.ar1450. This is a specific implementation that is uses an Aleko AR1450 sliding gate opener
   controller and motor to activate the roof. A single relay is used with this motor controller to work with
   its single push button interface. The motor direction alternates with each use. Mechanisms provided by the
   Aleko controller determine when the roof movement stops.

3. This user's example provides local control buttons in addition to remote control from the driver. It uses a bank of
   relays to switch polarity to the motor to control direction. An auxiliary function from the driver is used to
   remotely turn on power for the motor. This Arduino code maintains the state and monitors local switches to determine
   when to stop the motor.
 
The communication uses a simple text protocol so the controller could be something other than an Arduino.
The example driver code supports up to 4 output functions and 4 input functions.
The driver output functions will typically close relays to active the roof open and close.
The driver input request is to read the state of switches from the Arduino to detect when the roof is fully open
or fully closed.

Driver: RollOff ino
Executable: indi_rolloffino
INDI:       1.8.1


Outline of the text between roof driver and Arduino
---------------------------------------------------
Lines of text sent and received
Format: "(command:target:value)"

Command:  CON        Establish connection with Arduino
          GET        Get status of a switch
          SET        Set relay closed or open
Response: ACK        Success returned from Arduino
          NAK        Failure returned from Arduino

state:   OPENED | CLOSED | LOCKED | AUXSTATE

action:  OPEN | CLOSE | ABORT | AUXSET

Value:   ON | OFF | 0 | text-message

                 Driver                     Arduino
                 ------                     -------
                 (CON:0:0)            >                       
                                      <     (ACK:0:0) | (ACK:0:version) | (NAK:ERROR:message)
                 
Read switch      (GET:state:0)        >      
                                      <     (ACK:state:ON|OFF) | (NAK:ERROR:message)

Set relay        (SET:action:ON|OFF)  >
                                      <     (ACK:action:ON|OFF) | (NAK:ERROR:message)

