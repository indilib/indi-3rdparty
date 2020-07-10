Celestron AUX protocol driver
-------------------------------------

Authors: Paweł T. Jochym <jochym@wolf.ifj.edu.pl>
         Fabrizio Pollastri <https://github.com/fabriziop>

*Do not use this driver unattended! This is BETA software! Always have your power switch ready! It is NOT SUITABLE for autonomous operation yet!*
*There are no slew limits implemented at this stage.*

This is eqmod-style driver for the NexStar and other Celestron AUX-protocol 
mounts. It works over serial link to PC/AUX ports or HC serial port
and over WiFi to the NexStar-Evolution or SkyFi-equipped mount. 

The driver is in the beta stage.
It is functional and should work as intended but it is not complete.
I am using it and will be happy to help any brave testers.
I of course welcome any feedback/contribution.

What works:
- N-star alignment (with INDI alignment module)
- Basic tracking, slew, park/unpark
- GPS simulation. If you have HC connected and you have active gps driver 
  it can simulate Celestron GPS device and serve GPS data to HC. Works quite 
  nicely on RaspberryPi with a GPS module. You can actually use it as 
  a replacement for the Celestron GPS.
- Cordwrap control

What does not work/is not implemented:
- Joystick control
- Slew limits
- HC interaction (tracking HC motor commands to function as joystick)
- Probably many other things

Install
-------

The driver is not included in the PPA distribution yet - due to its beta 
state. So to use it you need to compile it from the source yourself.

You can make a stand-alone compilation or build the debian packages for your 
system. It should be fairly easy. Let me know if something in the following
guide is wrong or if you have problem with compiling the driver.

Get the source
==============

You can get the source from the SVN repository of the system on sourceforge
maintained by the INDI project (see the website of the project) or get it
from the github mirror of the sourceforge repository maintained by the author 
of this driver. Both will do fine. The github repository lets you track the 
development of the driver more closely in the nse branch of the repository, 
since only master branch is uploaded back to the upstream SVN repository.

- Make some working directory and change into it.
- Get the source from the github master branch - it takes a while 
  the repo is 64MB in size. 

Compiling on any linux system
=============================

The compilation is simple. You will need indi libraries installed. The best way
is to install libindi-dev package from the PPA. You may also want to have
indi-gpsd and gpsd packages installed (not strictly required). If you cannot use
the PPA you need to install libindi-dev from your distribution or compile the
indi libraries yourself using instructions from the INDI website. I have not
tested the backward compatibility but the driver should compile and work at
least with the 1.8.4 version of the library. My recommendation: use PPA if you
can. To compile the driver you will need also: cmake, cdbs, libindi-dev,
libnova-dev, zlib1g-dev. Run following commands (you can select other install
prefix):

```sh
mkdir -p ~/Projects/build/indi-caux
cd ~/Projects/build/indi-caux
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty/indi-celestronaux
make
```
You can run `make install` optionally at the end if you like to have the driver 
properly installed.


Building debian/ubuntu packages
===============================

To build the debian package you will need the debian packaging tools: 
`build-essential, devscripts, debhelper, fakeroot`

Create `package` directory at the same level as indilib directory with the 
cloned source. Then execute:

```sh
mkdir -p ~/Projects/build/deb-indi-caux
cd ~/Projects/build/deb-indi-caux
cp -r ~/Projects/indi-3rdparty/indi-celestronaux .
cp -r ~/Projects/indi-3rdparty/debian//indi-celestronaux debian
cp -r ~/Projects/indi-3rdparty/cmake_modules indi-celestronaux/
fakeroot debian/rules binary
fakeroot debian/rules clean
```
this should produce two packages in the main build directory (above `package`),
which you can install with `sudo dpkg -i indi-celestronaux_*.deb`.

