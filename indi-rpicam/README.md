# indi_rpicam
This is an INDI driver for Raspberry Pi High Quality Camera, the V2 camera (imx219 chip) and the V1 camera (omv5647 chip).

## Long exposures
For long exposures to work one must first first grab a waste-frame with the desired exposure time.
Once this is done, the exposure works as usual.

- rpi forum: https://www.raspberrypi.org/forums/viewtopic.php?f=43&t=63276&p=1773068&hilit=%22long+exposure%22#p1773068
- raspicam on the subject: https://www.raspberrypi.org/documentation/usage/camera/raspicam/longexp.md

## CROSS COMPILATION

! Don't use yet. This method does not actually find the camera object for some reason. 
  Most likely some library is forced to be loaded completely.

In order to develop and crosscompile this package for raspberry on a regular Linux box running Debian,
use following procedure:

- First install the packages:  g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf

- All development packages for INDI-development must be installed on a master Raspberry.

- Copy all necessary libraries and headerfiles using the script rsync-from-pi.sh, argument should be host or ip of the master raspberry

- Configure cmake using the extra option: -DCMAKE_TOOLCHAIN_FILE=path/to/rpi-crosscompile.cmake. In QtCreator this is done in via "Manage Kits" project settings.

- Build. 
