# ASI Power
ASI Power provides the INDI driver for controlling power distribution from a Raspberry Pi usinng the GPIO pins.
It is specifically configured to control the power panel on ASIAir Pro devices which use GPIO 12, 13, 26 and 18.

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
In addition you need to install pigpio from http://abyz.me.uk/rpi/pigpio/download.html as follows:
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
The driver uses the pigpiod daemon. and pigpio library 
In order to run as non-root user the pigpiod daemon must be running
pigpio is used rather than libgpiod as pigpio provides PWM functionality
Run the pigpiod daemon as root using systemd:
```
sudo cp pigpiod.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/pigpiod.service
sudo systemctl daemon-reload
sudo systemctl enable pigpiod.service
sudo systemctl start pigpiod.service
```
Alternatively, edit rc.local.

Start indiserve with the driver
```
indi_server indi_asi_power
```
