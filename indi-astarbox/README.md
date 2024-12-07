# AStarBox
AStarBox provides the INDI driver for controlling power disribution for the AStarBox.

Features:
  - Allows individual ports to be turned on and off
  - Dew heaters can be controlled in increments of 5%
  - Displays input voltage
  - Settings are peristent - will be re-established when AStarBox reboots

# Source
* https://github.com/indilib/indi-3rdparty.git

# Requirements
* INDI available here http://indilib.org/download.html
* CMake >= 3.0

# Installation
For normal use install from the INDI PPA with:
```
sudo apt install indi-astarbox
```

To build from source you need to install required libraries and headers before compiling. See [INDI site](http://indilib.org/download.html) for more details.

```
cd ~/Projects
git clone https://github.com/indilib/indi-3rdparty.git
cd indi-3rdparty
```

Compile and install the driver
```
mkdir -p ~/Projects/build/indi-astarbox
cd ~/Projects/build/indi-astarbox
cmake -DCMAKE_INSTALL_PREFIX=/usr ~/Projects/indi-3rdparty/indi-astarbox
make -j4
sudo make install
```

# How to use it?
Ensure the I2C interface is enabled on the Pi. Select AStarBox from the "Other" menu in Ekos.

Once connected, use the tick boxes on the Main Control tab to turn the power ports on and off.

Set the power for the dew ports using the sliders, then press Set.

The settings are persistent. Active ports will remain on after the AStarBox is powered off until the power is removed. Once the PI is rebooted, the active ports will be turned on even without running Indi.
