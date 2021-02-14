# ASI Power
ASI Power provides the INDI driver for controlling the  power distribution panel on ASIAir Pro devices

Features:
  - Select device type to determine whether it is On/Off or PWM controlled
  - Support for Camera, Focuser, Dew Heater, Flat Panel
  - PWM control in increments of 1%

# Source
https://github.com/ken-self/indi-3rdparty/indi-asi-power

# Requirements
* INDI available here http://indilib.org/download.html
* CMake >= 2.4.7

# Installation
You need to download and install required libraries before compiling. See [INDI site](http://indilib.org/download.html) for more details.
In addition you need to install pigpio from http://abyz.me.uk/rpi/pigpio/download.html
```
sudo apt install python-setuptools python3-setuptools
wget https://github.com/joan2937/pigpio/archive/master.zip
unzip master.zip
cd pigpio-master
make
sudo make install
```
Compile the driver:
```
git clone https://github.com/ken-self/indi-3rdparty.git
cd indi-3rdparty
git checkout asipower
cd ..
mkdir /p build/indi-asi-power
cmake -DCMAKE_INSTALL_PREFIX=/usr ~/Projects/indi-3rdparty/indi-asi-power
make
```
Install the driver:
```
sudo make install
```


# How to use it?
The driver interacts with the pigpiod daemon. Start the daemon using systemd or rc.local depending on you OS. pigpiod needs to run as root
The start the driver with indiserver
The driver connects to 4 GPIO ports:
GPIO 12
GPIO 13
GPIO 18
GPIO 26
Each port can act as a simple on/off switch or as a PWM output
```
indi_server indi_asi_power
```
