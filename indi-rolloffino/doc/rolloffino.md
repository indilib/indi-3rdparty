INDI roll-off roof driver using an Arduino to open and close the roof.

A roll-off roof driver for a DIY automation project that uses an Arduino to provide the interface with a roof motor or roof controller. The driver is intended to be installed and used without the need for coding in the INDI environment. It is the Arduino code that works from a standard interface with the driver that requires adaptation to control the selected motor. There is project, hardware and coding help available online for the Arduino. The driver provides an interface sufficient to open, close and determine if the roof is fully opened or fully closed for a roll-off or similar roof.

![components](components.png)

The driver as with other INDI drivers can be on the same host as the Ekos client or remote on a different computer located in the observatory. The Arduino can be local to the roll-off roof driver using a USB connection. The Arduino can optionally be connected to the roll-off roof driver using a WiFi connection. The Arduino activates the circuits that control the roof motor. The motor control can be provided by a re-purposed commercial opener, or a controller suitably built or bought to match requirements for the selected motor.
