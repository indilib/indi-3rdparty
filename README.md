# INDI 3rd Party Drivers
[![Linux](https://github.com/indilib/indi-3rdparty/actions/workflows/linux.yml/badge.svg)](https://github.com/indilib/indi-3rdparty/actions)
[![MacOS](https://github.com/indilib/indi-3rdparty/actions/workflows/macos.yml/badge.svg)](https://github.com/indilib/indi-3rdparty/actions)

INDI 3rd party drivers include all the drivers not included by default in the INDI Core Library.

# Which drivers are in 3rd party?

Check the [Device Driver documentation](https://www.indilib.org/devices.html) for detailed information on how to operate each device driver.

All the driver documentation is community-contributed. Any updates or correction to the documentation in INDI library is greatly appreciated.

The following classes of devices are supported in the INDI 3rd party repository:

+ Cameras: CCDs, CMOS, and DSLR
+ Mounts.
+ Focusers.
+ Filter Wheels.
+ Domes.
+ GPS.
+ Adaptive Optics.
+ Radio Astronomy Receivers.
+ Spectrometers.
+ Weather Stations.
+ Auxiliary drivers.

# What decides if a driver belongs to INDI Core or INDI 3rd party?

If a driver meets any of the following criteria, then it needs to be included in the INDI 3rd party repository:

+ Any camera driver.
+ Any driver with external dependencies not satisfied by INDI core library.
+ Any driver that would like to remain independent from INDI core library release schedule.

Learn more about INDI:
+ [Features](http://indilib.org/about/features.html)
+ [Discover INDI](http://indilib.org/about/discover-indi.html)
+ [Supported Devices](http://indilib.org/devices/)
+ [Clients](http://indilib.org/about/clients.html)

# Building

Before building INDI 3rd party drivers. [INDI Core Library](https://github.com/indilib/indi) must be already installed on your system.

## Install Pre-requisites

On Debian/Ubuntu:

```
sudo apt-get -y install libnova-dev libcfitsio-dev libusb-1.0-0-dev zlib1g-dev libgsl-dev build-essential cmake git libjpeg-dev libcurl4-gnutls-dev libtiff-dev libfftw3-dev libftdi-dev libgps-dev libraw-dev libdc1394-dev libgphoto2-dev libboost-dev libboost-regex-dev librtlsdr-dev liblimesuite-dev libftdi1-dev libavcodec-dev libavdevice-dev
```

You may also add libindi-dev if needed :

```
sudo apt-get -y install libindi-dev
```

## Create Project Directory
```
mkdir -p ~/Projects
cd ~/Projects
```

## Get the code
To build INDI in order to run drivers, then it is recommended to perform a quick shallow clone that will save lots of bandwidth and space:
```
git clone --depth=1 https://github.com/indilib/indi-3rdparty
```
On the other hand, if you plan to submit a PR or engage in INDI driver development, then getting a full clone is recommended:
```
git clone https://github.com/indilib/indi-3rdparty
```

## 3rd Party Drivers vs. 3rd Party Libraries

Some of the drivers included in the INDI 3rd party repository require a library (usually made by the device manufacturer) to be installed before the driver can be built and installed.
For example indi-qsi depends on libqsi, so libqsi must be built and installed before attempting to build and install the indi-qsi driver.
Many of these libraries are included in the 3rd Party Repository and can be installed just like the drivers.

## Building individual 3rd Party Drivers (typically recommended)

While you can build all the 3rd party drivers and libraries at once, it is typically recommended to build/install each 3rd party driver as required, since
each 3rd party driver may have its own pre-requisites and requirements, and you probably don't need all the drivers. 
If you want build and install just one driver, please see this example of how to build INDI EQMod driver:

```
mkdir -p ~/Projects/build/indi-eqmod
cd ~/Projects/build/indi-eqmod
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty/indi-eqmod
make -j4
sudo make install
```

The complete list of system dependancies for all drivers on Debian / Ubuntu

```
sudo apt-get install libnova-dev libcfitsio-dev libusb-1.0-0-dev zlib1g-dev libgsl-dev build-essential cmake git libjpeg-dev libcurl4-gnutls-dev libtiff-dev libftdi-dev libgps-dev libraw-dev libdc1394-22-dev libgphoto2-dev libboost-dev libboost-regex-dev librtlsdr-dev liblimesuite-dev libftdi1-dev libgps-dev libavcodec-dev libavdevice-dev
```

You may also add libindi-dev if needed :

```
sudo apt-get -y install libindi-dev
```

## Building all the 3rd Party Libraries

You can build **all** the 3rd Party Libraries at once if you already have **all** of the requirements for each library installed.
To install all the libraries, you would want to use the BUILD-LIBS build flag.  If you want to build **all** 3rd Party drivers as described in the section below, you need to do this step first.
You can use the following commands to install all the libraries:

```
mkdir -p ~/Projects/build/indi-3rdparty-libs
cd ~/Projects/build/indi-3rdparty-libs
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug -DBUILD_LIBS=1 ~/Projects/indi-3rdparty
make -j4
sudo make install
```

## Building all the 3rd Party Drivers

You can build **all** the 3rd Party Drivers at once if you already have **all** of the requirements for each driver installed.
Before you attempt this, please make sure to install **all** the 3rd Party Libraries first.  Please see the section above.
After you have all the requirements, you can run the following commands to install all the drivers:

```
mkdir -p ~/Projects/build/indi-3rdparty
cd ~/Projects/build/indi-3rdparty
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty
make -j4
sudo make install
```

# Support

+ [FAQ](http://indilib.org/support/faq.html)
+ [Forum](http://indilib.org/forum.html)
+ [Tutorials](http://indilib.org/support/tutorials.html)

# Development

+ [INDI API](http://www.indilib.org/api/index.html)
+ [INDI Developer Manual](http://indilib.org/develop/developer-manual.html)
+ [Tutorials](http://indilib.org/develop/tutorials.html)
+ [Developers Forum](http://indilib.org/forum/development.html)
+ [Developers Chat](https://riot.im/app/#/room/#kstars:matrix.org)
+ Sample drivers are available under examples and drivers/skeleton directories. They can be used as a starting point for your driver development.

### Code Style

INDI uses [Artistic Style](http://astyle.sourceforge.net) to format all the C++ source files. Please make sure to apply the following astyle rules to any code that is submitted to INDI. On Linux, you can create ***~/.astylerc*** file containing the following rules:
```
--style=allman
--align-reference=name
--indent-switches
--indent-modifiers
--indent-classes
--pad-oper
--indent-col1-comments
--lineend=linux
--max-code-length=124
```

Some IDEs (e.g. QtCreator) support automatic formatting for the code everytime you save the file to disk.

### How to create Github pull request (PR)

[How to contribute to INDI full guide](http://indilib.org/develop/tutorials/181-how-to-contribute-to-indi-github-development.html)

Here is the short version on how to submit a PR:
1. Login with a Github account and fork the official INDI repository.
2. Clone the official INDI repository and add the forked INDI repository as a remote (git remote add ...).
3. Create a local Git branch (git checkout -b my_branch).
4. Work on the patch and commit the changes.
5. If it is ready push this branch to your fork repo (git push -f my_fork my_branch:my_branch).
6. Go to the official repo's github website in a browser, it will popup a message to create a PR. Create it.
7. Pushing updates to the PR: just update your branch (git push -f my_fork my_branch:my_branch)..

If you would like to make cleaner PR (recommended!) please read this [tutorial](https://blog.adamspiers.org/2015/03/24/why-and-how-to-correctly-amend-github-pull-requests/) and follow it. The best way is to keep *one logical change per commit* and not pollute the history by multiple small fixes to the PR.

### Driver Documentation

When submitting a new driver, the driver user **documentation** is required as part of the submission process.

* Installation: Driver name, executable name, version, required INDI version.
* Features: What features does it support exactly?
* Operation: How to operate the driver? Each sub section should come with a screen shot of the various tabs..etc.
  Preferably annotated to make it easier for new users to follow.
  * Connecting: How to establish connection? How to set port if any?
  * Main Control: Primary control tab and its functions.
  * Options: Explanation for the various options available.
  * Etc: Any other tabs created by the driver.
* Issues: Any problems or issues or warnings the users should be aware about when using this driver.

# MacOS 3rd Party Binary Libraries and the FIX_MACOS_LIBS option

Some binary files in this repository are dynamic libraries provided by various manufacturers and/or are built by various people who are volunteers.  
The binary library files have install names that are then used when building and installing software on MacOS to properly link the 
libraries to the programs or to each other. The problem is that for various reasons, the install names sometimes are based on 
the build folders or install methods that those individuals used on their own computers and when the software is built by others, or 
built using different methods such as craft, the incorrect install names cause the linking to fail.

In addition, some of these libraries were built on computers that have other libraries those libraries depend on, such as libusb and opencv, 
and they have already been linked to these libraries in the locations on those people's computers.  Often those links are to homebrew versions or to build folders on those people's computers 
and those libraries might not exist in those same locations on the craft binary server or computer on which someone is using this repository to build and install indi libraries.  

Both of these issues would cause the libraries to fail to be installed and linked properly, even if it appears to have succeeded.  
This will cause them to not function correctly and possibly cause crashes of INDI or KStars.  
Often the issue would not be revealed until the packaging step or even when someone tries to use the drivers. 

The Fix MacOS Libraries Macro will try to detect both of these issues and if it finds a problem, it will cause a fatal error.
This way, the issue will get flagged so that the managers of this repository will be alerted to the issue either by people who use this repository or by the craft binary server.
If this happens to you, please report the issue as soon as possible so that the repository can be updated.

The FIX_MACOS_LIBS option can be enabled to fix the problems immediately when they are detected. If you enable this option, the build will continue after the problem is fixed so 
you can fully build the software.  If you have the rights to update the INDI 3rd Party Repository, please fix the problem by just doing a commit and pushing the changes the script made 
to the libraries.  If not, again, please report the problem.  This build option cannot be enabled on the craft binary server because it will leave the repository on the server in a modified 
state and the next time the 3rd Party Repository gets modified, it will fail to do a "git pull" due to the modifications.

# Unit tests

In order to run the unit test suite you must first install the [Google Test Framework](https://github.com/google/googletest). You will need to build and install this from source code as Google does not recommend package managers for distributing distros.(This is because each build system is often unique and a one size fits all aproach does not work well).

Once you have the Google Test Framework installed follow this alternative build sequence:-

```
cd ~/Projects/build/indi-3rdparty
cmake -DINDI_BUILD_UNITTESTS=ON -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty
make -j4
cd test
ctest -V
```
