# RPi GPIO
RPi GPIO provides the INDI driver for controlling up to 5 GPIO pins on a Raspberry Pi. Typically each GPIO pin
would be connected to an optoisolator circuit or RPi hat to control larger voltages and current

Features:
  - Select device type to determine whether it is On/Off or PWM controlled
  - PWM control in increments of 1%
  - Support for a sequence of timed pulses any pin to control e.g. DSLR shutter and focus/half-shutter
  - Support for Active Low operation

# Source
* https://github.com/indilib/indi-3rdparty.git
* https://github.com/joan2937/pigpio

# Requirements
* INDI available here http://indilib.org/download.html
* CMake >= 3.0

# Installation
For normal use install from the INDI PPA with:
```
sudo apt install indi-rpi-gpio
```
This is dependent on the pigpiod daemon which normally runs as a systemd service. The daemon and service are installed with INDI package libpigpiod

To build from source you need to install required libraries and headers before compiling. See [INDI site](http://indilib.org/download.html) for more details.
In addition you need to install pigpio client libraries and headers
```
sudo apt -y install libpigpiod-if-dev libpigpiod-if2-1 pigpio-tools libpigpiod

cd ~/Projects
git clone https://github.com/indilib/indi-3rdparty.git
cd indi-3rdparty
```
Compile and install the driver
```
mkdir -p ~/Projects/build/indi-rpi-gpio
cd ~/Projects/build/indi-rpi-gpio
cmake -DCMAKE_INSTALL_PREFIX=/usr ~/Projects/indi-3rdparty/indi-rpi-gpio
make
sudo make install
```
# How to use it?
The driver uses the pigpiod daemon and library . Refer to http://abyz.me.uk/rpi/pigpio/

Start indiserver with the driver
```
indi_server indi_rpi_gpio
```
