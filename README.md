# INDI 3rd Party Drivers
[![Build Status](https://travis-ci.org/indilib/indi.svg?branch=master)](https://travis-ci.org/indilib/indi)
[![CircleCI](https://circleci.com/gh/indilib/indi.svg?style=svg)](https://circleci.com/gh/indilib/indi)

INDI 3rd party drivers include all the drivers not included by default in the INDI Core Library.

What decides if the driver needs to belong to the 3rd party repository?
+ All cameras drivers.
+ Any drivers with external dependencies not satisfied by INDI core library.
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
sudo apt-get install libnova-dev libcfitsio-dev libusb-1.0-0-dev zlib1g-dev libgsl-dev build-essential cmake git libjpeg-dev libcurl4-gnutls-dev libtiff-dev libfftw3-dev libftdi-dev libgps-dev libraw-dev libdc1394-22-dev libgphoto2-dev libboost-dev libboost-regex-dev librtlsdr-dev liblimesuite-dev libftdi1-dev
```
## Get the code
If you plan on contributing to INDI development, then it is recommended to perform a full clone:
```
git clone https://github.com/indilib/indi-3rdparty.git
```

If on the other hand, you are only interested to build INDI 3rd party driver, then it is best to clone a shallow copy as this will be **much** faster and saves lots of space:
```
git clone --depth=1 https://github.com/indilib/indi-3rdparty
```

## Build 3rd party drivers

You can build the all the 3rd party drivers at once (not recommended) or build the required 3rd party driver as required (recommended). Each 3rd party driver may have its own pre-requisites and requirements. For example, to build INDI EQMod driver:

```
cd build
mkdir indi-eqmod
cd indi-eqmod
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ../../3rdparty/indi-eqmod
make
sudo make install
```

The complete list of system dependancies for all drivers on Debian / Ubuntu

```
sudo apt-get install 
```

To build **all** 3rd party drivers, you need to run cmake and make install **twice**. First time is to install any dependencies of the 3rd party drivers (for example indi-qsi depends on libqsi, so libqsi must be built and installed first), and second time to install the actual drivers themselves.

```
cd build
mkdir indi-3rdparty
cd indi-3rdparty
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ../../indi-3rdparty
make
sudo make install
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ../../indi-3rdparty
make
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

### Sample Drivers

You can base a new driver from an existing driver. Look in either the examples or drivers/skeleton directories on how to get started.

# Unit tests

In order to run the unit test suite you must first install the [Google Test Framework](https://github.com/google/googletest). You will need to build and install this from source code as Google does not recommend package managers for distributing distros.(This is because each build system is often unique and a one size fits all aproach does not work well).

Once you have the Google Test Framework installed follow this alternative build sequence:-

```
mkdir -p build/indi-3rdparty
cd build/indi-3rdparty
cmake -DINDI_BUILD_UNITTESTS=ON -DCMAKE_BUILD_TYPE=Debug ../../indi-3rdparty
make
cd test
ctest -V
```

For more details refer to the scripts in the travis-ci directory.
