### Description

An Observatory roll off roof driver to automate the opening and closing of a roll off roof in the INDI environment. The driver interacts with the INDI Observatory Dome, Mount and Weather capabilities. The driver communicates with an Aurdino microcontroller using a USB or WiFi connection. The driver sends open or close commands and requests from the Arduino whether the roof is opened or closed. The driver does not know how the roof is being operated.

It is the Arduino that provides the appropriate code to match the hardware that controls the roof. Some Arduino source examples are provided. A common approach is to use a commercial gate or garage door opener to contol the roof. These openers provide the capability to sense when the roof is fully open or fully closed and can stop the motion at that point. They usually have a button or buttons to manually open and close the roof. Then a relay can be used instead of, or in addition to, the button to automate the operation. The standard Arduino Uno example can be used for this kind of controller. If the default selection of Arduino pins for operating the relay are used then no source editing would be needed.

[More information is in the documentation file.](doc/rolloffino.md)

### Driver
```
Driver:       RollOff ino
Executable:   indi_rolloffino
Minimum INDI: 2.0.1
```

### Installation

The driver will be installed and ready for use after an indi-full install which includes 3rdparty drivers.

### Building

To build The driver from source, refer to the directions in the parent directory under the topic "Building".  https://github.com/indilib/indi-3rdparty. The directions describe the required build environment, include the installation of libindi-dev. In the section "Building individual 3rd Party Drivers" replace references to indi-eqmod with indi-rolloffino. 

### Usage

Available for selection in a KStars/Ekos INDI client. The driver requires successful communication to an already running Arduino to complete the on-line connection stage. The driver can be manually started from the command line by the indiserver:
`$ indiserver indi_rolloffino`.

### Source Files

Refer to the general documentation in the parent directory for 3rdparty drivers https://github.com/indilib/indi-3rdparty [ indi-rolloffino ].

The source files can best be obtained by cloning the 3rdparty repo as described in the general documentation in the parent directory for 3rdparty drivers https://github.com/indilib/indi-3rdparty. Navigate to the indi-rolloffino directory. Documentation can be read and individual Arduino examples obtained by using a web browser accessing https://github.com/indilib/indi-3rdparty. The Green "Code" button provides one way to get repository files. Alternatively navigate to indi-rolloffino folder and right click an individual file to save it in a local directory.
