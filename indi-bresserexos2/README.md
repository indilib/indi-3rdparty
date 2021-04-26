# Exos II GoTo Telescope Mount Driver for libindi

---

## Disclaimer
You get the driver free of charge and, also may modify it to your needs.

However the software is distributed AS IS.

**I'M NOT RESPONSIBLE FOR ANY DAMAGES OR INJURIES, CAUSED BY THIS SOFTWARE!**

---

## Features of the Driver
- Works with KStars/Stellarium using Indi Connection
- GoTo Coordinates and Track commands (Sidereal Tracking only)
- Park and Abort commands
- Sync command for alignment of Software Sky and actual pointing
- Get/Set Site Location
- Set Date/Time
- Adjust Pointing while Tracking

---

## Introduction
This is a basic driver for the Bresser Exos II GoTo telescope mount controller, allowing the connection to Indi clients/software.
The driver is intended for remote control on a Raspberry Pi, running Astroberry with libindi, but may run on any Indi running platform.
Its current state is experimental, but hopefully gradually improves.
Since its the initial release, feedback for improvement is appreciated.

If your have an improvements, features to add or a bug to report, please fell free to write a mail, a ticket in the issues section or a pull request.

### About the Mount
The Bresser Exos II GoTo Mount has a relabled JOC SkyViewer Handbox (PCB Rev. 1.09 2012_08), there are several other versions handbox revisions out there.
It runs the Firmware Version 2.3 distributed by Bresser.
The mount is quite autonomous, in terms motion controls, when initialized properly no jams or crashes where noticed.
On the serial protocol side however, this device is quite primitive. 
The data exchange is established using a 13 Byte message frame, with a 4 Byte preamble, leaving 1 byte for a command and 8 bytes for command parameter data.
The protocol only accepts, a few commands for goto, sync, parking, motion stop and Location/Time/Date setting.
The Device is not very talkative, it only sends responses to location command, and only reports back the its current pointing coordinates.
This makes it difficult to assess the state of the mount.
Also this introduces some limitations which competative products may not have.µµ

The serial protocol was reverse engineered using serial port sniffing tools, developping this driver as a result. 

---

## Requirements
***This driver is intended for Bresser Exos II GoTo System not the Explore Scientific Exos II which may share some similiarities. The drivers for these system and its derivitives are already included in the INDI Environment.***

- Raspberry Pi with Astroberry (AB, Version 2.0.3 or higher) with Libindi 1.8.7 (https://www.astroberry.io/, https://www.indilib.org/), In fact: any platform running indi will do, but I developped this with AB in mind,
- latest Version of cmake installed (at least Version 3.13)
- Astronomy Software (KStars, for Windows download see: https://edu.kde.org/kstars/#download, or use the package manager of your linux distribution.)
- A COM Port or working USB to Serial Adapter (any device supporting the change of Baud Rates will do!)
- The Bresser Serial Adapter for the Handbox (https://www.bresser.de/Astronomie/Zubehoer/Motoren-Steuerungen/BRESSER-Computer-Kabel-zur-Fernsteuerung-von-MCX-Goto-Teleskopen-und-EXOS-II-EQ-Goto-Montierungen.html)
- The Bresser Exos II GoTo Mount (or the Upgrade Kit) (https://www.bresser.de/Astronomie/BRESSER-Messier-EXOS-2-EQ-GoTo-Montierung.html, https://www.bresser.de/Astronomie/Zubehoer/Motoren-Steuerungen/BRESSER-StarTracker-GoTo-Kit.html)
- The EQ mount AZ/ALT version is not supported!
- Firmware Version 2.3 installed on the Handbox (https://www.bresser.de/Astronomie/Zubehoer/Motoren-Steuerungen/BRESSER-Computer-Kabel-zur-Fernsteuerung-von-MCX-Goto-Teleskopen-und-EXOS-II-EQ-Goto-Montierungen.html, under Manual)

## Known Issues and Limitations
- Tracking modes can not be set, only Sidereal Tracking is working right now.
- KStars only updates location but not the time, everything is implemented, but the function is not called by the EKOS Software.
- More a Hint than an issue: Sync only works when tracking an object. This behaviour is implemented on the handbox and can not be changed.
- The sync function only works if you changed the pointing with your hand box not via the EKOS pointing control
- you can not perform the meridian flip from afar, since the handbox does not allow it.
- Software Sky Coordinates may differ from Handbox Coordinates
- Newer versions of indi (Version 1.8.8) may break the build, since the driver interface has changed.

---

## Getting started

- See [Driver installation](Documentation/Installation.md) for installing  the driver.
- See [Application setup](Documentation/ApplicationSetup.md) how to setup the driver in observatory software.
- See [Troubleshooting](Documentation/Troubleshooting.md) for advice on how to resolve common issues.
- See [FAQ](Documentation/FAQ.md) for common questions.
---

## Important Note before Further Setup or Observation
It is **important** that you put the scope in the Home position, Polar and Star Align in accordance to the Bresser manual provided with the telescope and mount.

**Its vital in order to avoid damage to your Equipment. This Driver can not handle this for your!**

**Also do not point your Telescope directly to the sun, using this driver. It only handles coordinates not objects, and will there for no prevent you from doing so!**

---

## Thanks
- Thanks to spitzbube for his effort in reverse engineering the handbox (https://github.com/Spitzbube/EXOS-2_GoTo_HandController) for revealing valuable insights!
- Thanks to SimonLilie from https://forum.astronomie.de for feedback and testing!
