Note: Original project of sebo-b https://github.com/sebo-b/TicFocuser-ng

# TicFocuser-ng

INDI Driver for USB focuser based on Pololu Tic controller.

This project was initially a fork of TicFocuser driver written by Helge Kutzop and published on [GitHub](https://github.com/HelgeMK/TicFocuser) as it seemed that the author is not maintaining it anymore. However, the refactoring of code was such deep, that I'm not sure if a single line of the original code stayed in this version. Helge was basing his version on Radek Kaczorek's "astroberry-diy" drivers published on [GitHub](https://github.com/rkaczorek/astroberry-diy). As said I'm quite certain that all old code was rewritten, nevertheless to respect both authors I kept their names in AUTHORS file.

# Whys

You may wonder why we have created another focuser as there are many already available on the market. Here are my reasons for it:
* DIY focuser is much cheaper than a manufactured one.
* DIY is more fun.
* I wanted something small and simple for astrophotography, without an external controller.
* I wanted a focuser which can be connected directly to a computer (PC, Laptop or RaspberryPi), so USB seemed to be the best choice.
* I wanted something aesthetic.

Pololu Tic controller fulfills all these needs. It is relatively cheap, small and among others, it supports USB (no need for GPIOs or extra Arduino).

# Software

This is a driver for a great framework: INDI. INDI together with KStars is opensource, works natively on Linux and MacOS, and can fully control astrophotography sessions. I was using it on RaspberryPi 3B+ and later on, I switched to much more powerful Intel NUC mini-pc. 

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

In general, you should use released version instead of git bleeding-edge work. The latest release was v1.0-rc.

```
$ curl -L https://github.com/sebo-b/TicFocuser-ng/archive/v1.0-rc.tar.gz | tar -xz
$ cd TicFocuser-ng-1.0-rc
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
$ make
$ sudo make install
```
To automatically create Arch package you can use AUR package created by myself available [here](https://aur.archlinux.org/packages/libindi-ticfocuser/).

### Configure Pololu Tic

If you haven't done so, execute `ticgui` and properly configure Tic controller. For various reasons, motor parameters are not controlled by TicFocuser driver.

At minimum:
* Configure your motor parameters like: current limit, step mode, speed, acceleration, etc.

![ticcmd](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/ticgui_motorsettings.png)

* Uncheck `Enable command timeout` checkbox in the `Serial` box. More info abt it in [chapter 4.4 of Pololu Tic documentation](https://www.pololu.com/docs/0J71/all#4.4). 

* Test from `ticgui` if your focuser works correctly.

You can read more about setting up proper motor parameters in [chapter 4.3 of Pololu Tic documentation](https://www.pololu.com/docs/0J71/all#4.3). In my setup I'm using current limit much lower than the rate of my motor because not much force is needed for my focuser, I save power and what's most important vibrations caused by the motor are marginalized.

### Configure KStars

Add `Tic Focuser NG` to INDI profile in KStars and you are good to go.

# Hardware

### What will you need
1. Tic Motor controller (without gold pins). I used T825 which looks like that:

![T825](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/tic825.jpg)

2. NEMA-17 stepper motor. I'm using an equivalent of [Pololu #2267](https://www.pololu.com/product/2267).

![NEMA17](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/nema17.jpg)

As I don't relly need much of torque recently I have also bought a much thinner NEMA-17 motor. Model name is 17Hs08-1004S.

![NEMA17_Small](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/nema17_small.jpg)

3. In case you prefer Bluetooth over USB connection, you need a Bluetooth module (find more in the next section).

![DX-BT18](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/dx-bt18.jpg)

4. Possibility to 3D print the case.
5. DC Barrel Power Jack Socket 5.5mm/2.1mm.

![DC](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/dc_barrel.jpg)

6. Basic soldering skills.
7. A way to connect the stepper motor to your focuser.

### Bluetooth module

Focuser doesn't require a high bitrate, and Bluetooth is built into PC (Intel NUC) which I'm using. Also, RaspberryPi has it onboard. So I thought that getting rid of one cable is worth the effort.

Most of the BT modules supporting SPP profile should basically work. However, HC-06 is probably the most popular one. What you need to pay attention for is that UART on Pololu Tic is 5V logic. What's more, Pololu in the documentation is stating:

> The serial interface uses non-inverted TTL logic levels: a level of 0 V corresponds to a value of 0, and a level of 5 V corresponds to a value of 1. The input signal on the RX pin must reach at least 4 V to be guaranteed to be read as high, but 3.3 V signals on RX typically work anyway.

The main concern I had was if 3.3V (5V tolerant) module will actually be able to communicate directly with Tic, what exactly "typically work anyway" mean. I have checked the specification of Pic (SoC used on Tic Controller), and it is clear there that 4V is the minimum voltage for high level. Nonetheless, Pololu also specifies that TX/RX are pulled up.

> The Tic’s RX pin is an input, and its TX pin is an output. Each pin has an integrated 100 kΩ resistor pulling it up to 5 V and a 220 Ω or 470 Ω series resistor protecting it from short circuits.

So my assumption was that Pololu could not clearly state 3.3V tolerant, as Pic doesn't tolerate it. However, pulled up pin should do the job. I was apparently right because the module I have is able to communicate with Tic Controller.

HC-05 and HC-06 are probably the most popular Bluetooth 2.0 to SPP modules. I, however, went for a different one, DX-BT18. It looked very solid, had FCC certification (and English documentation available on FCC site), it supports dual-mode (maybe in the future I add BLE connection) and was extreamly cheap ($2.10 on Aliexpress). Of course, you need the one with base-board as it has LDO from 6V to 3.3V.

I have setup baud-rate on the module and Tic to 115200, and it works like a charm. Just one note, documentation says that 115200 is the 4th mode: `AT+BAUD=4`. 4th mode is actually 57600, to set up 115200 I had to use 5th, undocumented mode: `AT+BAUD=5`.

### 3D printed case

NOTE: this is a version of the hardware without Bluetooth module. I'll publish an updated version when I finish it.

Model of controller case looks like the following:

![Case](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/case1.jpg)

Case after printout mounted on the motor:

![Case](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/case2.jpg)

With all elements inside:

![Case](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/case3.jpg)

And fully assembled and mounted on the tube:

![Case](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/focuser_final.jpg)

A project of the case is [attached as STL file](https://github.com/indilib/indi-3rdparty/blob/master/indi-TicFocuser-ng/doc/ticfocuser_case.stl) for 3D printer. 

Enjoy!

# Disclaimer

This software is working fine for me on Arch Linux with INDI v1.7.9 on x86 and Tic T825. I haven't tested it on RaspberryPi as I switched from this platform. This software is not changing any electrical parameters of Tic controller configuration. However, each software has bugs. I don't take any responsibility for it and you are using it at *your own risk*.
