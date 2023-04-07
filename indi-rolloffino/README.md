### Description

An Observatory rolloff roof driver to automate the opening and closing of a roll off roof in the INDI environment. When installed it will interact with the INDI Observatory Dome, Mount and Weather capabilities. The driver communicates with an Aurdino microcontroller using a USB or WiFi connection. The driver sends open or close commands and requests from the Arduino to know whether the roof is opened or closed. The driver does not know how the roof is being operated.

It is the Arduino that will need to provide the appropriate code to match the hardware that controls the roof. Some Arduino source examples are provided. A common approach is to use a commercial gate or garage door opener to contol the roof. These openers provide the capability to sense when the roof is fully open or fully closed and can stop the motion at that point. They usually have a button or buttons to manually open and close the roof. Then a relay can be used instead of, or in addition to, the button to automate the operation. The standard Arduino Uno example can be used for this kind of controller. If the default selection of Arduino pins for operating the relay are used then no source editing would be needed.

[More detailed information is in the documentation file.](doc/rolloffino.md)

### Installation




### Usage

### Building

### Requirements

### Obtaining Source Files

