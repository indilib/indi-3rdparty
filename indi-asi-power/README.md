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

```
# How to use it?
Enable 1-Wire interface using raspi-config or adding 'dtoverlay=w1-gpio' to /boot/configure.txt for temperature compensation support (reboot required). Run Kstars and select Astroberry Focuser (Focuser section) and/or Astroberry Relays (Aux section) and/or Astroberry System (Aux section) in Ekos profile editor. Then start INDI server in Ekos with your profile, containg Astroberry drivers. Alternatively you can start INDI server manually by running:
```
indi_server indi_asi_power
```
Start KStars with Ekos, connect to your INDI server and enjoy!

Note that your user account needs proper access right to /dev/gpiochip0 device. By default you can read/write only if you run driver as root or user who is a member of gpio group. Add your user to gpio group by running ```sudo usermod -a -G gpio $USER```

# What hardware is needed for Astroberry DIY drivers?

1. Astroberry Focuser
* A stepper motor
* Stepper motor controller - DRV8834 and A4988 are supported
  Starting from version 2.5 you can set your own BCM Pins on Options Tab!
  Default Motor Controller to Raspberry Pi GPIO wiring from v2.6 (changed!):
   - BCM23 / PIN16 - DIR
   - BCM24 / PIN18 - STEP
   - BCM22 / PIN15 - SLEEP + RST
   - BCM17 / PIN11 - M1/M0
   - BCM18 / PIN12 - M2/M1
   - BCM27 / PIN13 - M3/-

  Default Motor Controller to Raspberry Pi GPIO wiring before v2.6:
   - BCM04 / PIN7 - DIR
   - BCM17 / PIN11 - STEP
   - BCM23 / PIN16 - SLEEP + RST
   - BCM22 / PIN15 - M1/M0
   - BCM27 / PIN13 - M2/M1
   - BCM24 / PIN18 - M3/-

   Note: Make sure you connect the stepper motor correctly to the controller (B2, B1 and A2, A1 pins on the controller).
         Remember to protect the power line connected to VMOT of the motor controller with 100uF capacitor.
* DS18B20 temperature sensor connected to BCM4 / PIN7 for temperature reading and automatic temperature compensation
   Note: You need to use external 4k7 ohm pull-up resistor connected to data pin of DS18B20 sensor

2. Astroberry Relays
* Relay switch board eg. YwRobot 4 relay
  Default pins, each switching ON/OFF a relay (active-low). Starting from version 2.5 you can set your own BCM Pins on Options Tab!
   - BCM05 / PIN29 - IN1
   - BCM06 / PIN31 - IN2
   - BCM13 / PIN33 - IN3
   - BCM19 / PIN35 - IN4

   Note: All inputs are set to HIGH by default. Most relay boards require input to be LOW to swich ON a line.
