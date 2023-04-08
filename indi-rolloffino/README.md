### Description

An Observatory rolloff roof driver to automate the opening and closing of a roll off roof in the INDI environment. When installed it will interact with the INDI Observatory Dome, Mount and Weather capabilities. The driver communicates with an Aurdino microcontroller using a USB or WiFi connection. The driver sends open or close commands and requests from the Arduino to know whether the roof is opened or closed. The driver does not know how the roof is being operated.

It is the Arduino that will need to provide the appropriate code to match the hardware that controls the roof. Some Arduino source examples are provided. A common approach is to use a commercial gate or garage door opener to contol the roof. These openers provide the capability to sense when the roof is fully open or fully closed and can stop the motion at that point. They usually have a button or buttons to manually open and close the roof. Then a relay can be used instead of, or in addition to, the button to automate the operation. The standard Arduino Uno example can be used for this kind of controller. If the default selection of Arduino pins for operating the relay are used then no source editing would be needed.

[More information is in the documentation file.](doc/rolloffino.md)

## Driver
```
Driver:       RollOff ino
Executable:   indi_rolloffino
Minimum INDI: 1.8.1
```

### Installation

The driver will have been built and installed from having done a indi-full install which includes the 3rdparty drivers. If you want to build it from source refer to the general documentation in the parent directory under Building. https://github.com/indilib/indi-3rdparty. In the section "Building individual 3rd Party Drivers" replace references to indi-eqmod with indi-rolloffino. 

### Usage

Available from KStars/Ekos or compatible INDI client. It requires successful communication to the Arduino to complete the on-line connection stage.

It can be manually started from the command line by the indiserver:
`$ indiserver indi_rolloffino`.

### Requirements



### Obtaining Source Files

Refer to the general documentation in the parent directory for 3rdparty drivers https://github.com/indilib/indi-3rdparty

The source files can best be obtained by cloning the 3rdparty repo as described in the general documentation in the parent directory for 3rdparty drivers https://github.com/indilib/indi-3rdparty.
Then navigate to the indi-rolloffino directory. 

If you already have an installed rolloffino driver from an indi-full install. Then perhaps you are just looking for one of the Arduino examples to start working on the Arduino side of things. If so you can use your web browser accessing https://github.com/indilib/indi-3rdparty. The Green "Code" button provides one way to get repository files. You can also navigate to indi-rolloffino folder and right click an individual file to save it in one of your local directories.
