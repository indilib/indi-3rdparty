# libpigpiod
libpigpiod is a build of the pigpiod daemon using the GPIO control software pigpio
The daemon normally runs as a systemd service


# Source
* https://github.com/indilib/indi-3rdparty.git
* https://github.com/joan2937/pigpio

# Requirements
* INDI available here http://indilib.org/download.html
* CMake >= 3.0

# Installation
For normal use install from the INDI PPA with:
```
sudo apt install libpigpiod
```
This installs the the pigpiod service and enables and starts the pigpiod service

To build from source you need to install required libraries and headers before compiling. See [INDI site](http://indilib.org/download.html) for more details.
In addition you need to install pigpio client libraries and headers

The library and service are installed by default on Raspbian. To use the Raspbian package:
```
wget http://archive.raspberrypi.org/debian/pool/main/p/pigpio/pigpiod_1.71-0~rpt1_amd64.deb
wget http://archive.raspberrypi.org/debian/pool/main/p/pigpio/pigpiod_1.71-0~rpt1_arm64.deb
wget http://archive.raspberrypi.org/debian/pool/main/p/pigpio/pigpiod_1.71-0~rpt1_armhf.deb
```

```
sudo apt -y install libpigpiod-if-dev libpigpiod-if2-1 pigpio-tools

cd ~/Projects
git clone https://github.com/indilib/indi-3rdparty.git
cd indi-3rdparty
```
Compile and install the driver and pigpiod
```
mkdir -p ~/Projects/build/libpigpiod
cd ~/Projects/build/libpigpiod
cmake -DCMAKE_INSTALL_PREFIX=/usr ~/Projects/indi-3rdparty/libpigpiod
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
The pigpiod daemon and pigpio library enable drivers to run as non-root user
http://abyz.me.uk/rpi/pigpio/

