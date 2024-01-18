# TicFocuser-ng
INDI Driver for USB focuser based on Pololu Tic controller.

This is an integration of the project from Sebastian Baberowski (https://github.com/sebo-b/TicFocuser-ng), based on the work of Helge Kutzop (https://github.com/HelgeMK/TicFocuser)/

Check the upstream Sebastian Baberowski page for more informations/updates and the building process of this DIY Focuser.

I (Bruno Bzeznik) just copied the upstream and adapted Cmake confguration to integrate correctly into the latest indi-3rdparty drivers.

Here is a relevant extract from the original Readme file from upstream:

### Connections

The driver can utilize various connection interfaces to communicate with Pololu Tic controller. Some connections require particular libraries to be installed in your machine, CMake script will automatically detect these dependencies and enable only these connections, which can be compiled.

There are two separate libraries used to manage Tic protocol. These are [Pololu Tic USB C library](https://github.com/pololu/pololu-tic-software) and [Pololu Tic Arduino library](https://github.com/pololu/tic-arduino). Both libraries are developed by Pololu, but apparently they doesn't share the code. The first one can be used only for USB connection. The second one is more generic and can utilize serial connections. However, it is developed for Arduino and was probably not meant to be compiled on Linux. I have ported it to Linux and extended for Bluetooth and USB (it can also be extended to I2C, but so far there was no such a request from the community). 

Available connections:
1. PololuUSB - this is a USB connection which utilizes [Pololu Tic C library](https://github.com/pololu/pololu-tic-software). Check [dependencies](#Dependencies) section for more info about installing it.
2. LibUSB - this is a USB connection with utilizes standard libusb-1 library probably available on all Linux distributiins. It use modified [Pololu Tic Arduino library](https://github.com/pololu/tic-arduino) which is included in this repo. Only libusb-1 is needed to make it work.
3. Bluetooth - this is a serial Bluetooth connection. It also uses [Pololu Tic Arduino library](https://github.com/pololu/tic-arduino) and depends on standard libbluetooth.
4. Serial - this is a regular serial connection (after configuring rfcomm tty can be also used for Bluetooth). This connection is always compiled.

### Dependencies

Please note that you need these libraries for compilation, so on most Linux distributions you'll need *-dev versions of these.

1. You need INDI library installed together with headers. Most of Linux distributions have it in the repo. To install it on Arch simply type `pacman -S libindi`

2. PololuUSB - for this connection you'll need Pololu Tic library. You can find instructions on how to download it and compile on [Pololu site](https://www.pololu.com/docs/0J71). On Arch you can use AUR package created by myself available [here](https://aur.archlinux.org/packages/pololu-tic-software/).

3. LibUSB - for LibUSB based connection you need libusb-1. A lot of software depends on this library, so it is usually already installed in the system. If it is not installed, install it from your Linux distribution repository.

4. LibBluetooth - if you are going to use Bluetooth, you need libbluetooth. If it is not installed, install it from your Linux distribution repository.

### Compilation

```
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
$ make
$ sudo make install
```

### Configure Pololu Tic

If you haven't done so, execute `ticgui` and properly configure Tic controller. For various reasons, motor parameters are not controlled by TicFocuser driver.

At minimum:
* Configure your motor parameters like: current limit, step mode, speed, acceleration, etc.

![ticcmd](https://raw.githubusercontent.com/sebo-b/TicFocuser-ng/master/extras/ticgui_motorsettings.png)

* Uncheck `Enable command timeout` checkbox in the `Serial` box. More info abt it in [chapter 4.4 of Pololu Tic documentation](https://www.pololu.com/docs/0J71/all#4.4). 

* Test from `ticgui` if your focuser works correctly.

You can read more about setting up proper motor parameters in [chapter 4.3 of Pololu Tic documentation](https://www.pololu.com/docs/0J71/all#4.3). In my setup I'm using current limit much lower than the rate of my motor because not much force is needed for my focuser, I save power and what's most important vibrations caused by the motor are marginalized.

