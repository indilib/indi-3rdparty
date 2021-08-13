This package provides the INDI driver for the Orion StarShoot G3 cameras. It
expected that this will also work with Brightstar Mammut Lyuba cameras.

# Requirements
+ INDI
+ CFITSIO
+ libusb


# Installation
Go to the directory where  you unpacked indi_orion_ssg3 sources and do:

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install


# Running
Once installed, the Orion StarShoot G3 INDI driver can be used by an INDI client such as
KStars (Ekos) or OpenPHD. It can also be used manually by invoking it via the 
indi_server:
$ indi_server indi_orion_ssg3_ccd

