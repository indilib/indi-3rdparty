# ASI Power
ASI Power provides the INDI driver for controlling power distribution from a Raspberry Pi usinng the GPIO pins.
It is specifically configured to control the power panel on ASIAir Pro devices which use GPIO 12, 13, 26 and 18.

Features:
  - Select device type to determine whether it is On/Off or PWM controlled
  - Support for Camera, Focuser, Dew Heater, Flat Panel
  - PWM control in increments of 1%

# Source
* https://github.com/indilib/indi-3rdparty.git
* https://github.com/joan2937/pigpio

# Requirements
* INDI available here http://indilib.org/download.html
* CMake >= 3.0

# Installation
For normal use install from the INDI PPA with:
```
sudo apt install indi-asi-power
```
This installs the driver and the pigpiod service it requires; and enables and starts the pigpiod service

To build from source you need to install required libraries and headers before compiling. See [INDI site](http://indilib.org/download.html) for more details.
In addition you need to install pigpio client libraries and headers
```
sudo apt -y install libpigpiod-if-dev libpigpiod-if2-1 pigpio-tools

cd ~/Projects
git clone https://github.com/indilib/indi-3rdparty.git
cd indi-3rdparty
```
Compile and install the driver and pigpiod
```
cd ~/Projects/indi-3rd-party
git checkout asipower
mkdir -p ~/projects/build/indi-asi-power
cmake -DCMAKE_INSTALL_PREFIX=/usr ~/Projects/indi-3rdparty/indi-asi-power
make
sudo make install
```
Manually enable and start the pigpiod service with systemd
```
sudo systemctl daemon-reload
sudo systemctl enable pigpiod.service
sudo systemctl start pigpiod.service
```

# How to use it?
The driver uses the pigpiod daemon and pigpio library 
In order to run the driver as non-root user the pigpiod daemon must be running
pigpio library is used rather than libgpiod or wiringpi as pigpio provides PWM output
with accurate hardware timing on GPIO 0-31
http://abyz.me.uk/rpi/pigpio/

Start indiserver with the driver
```
indi_server indi_asi_power
```
