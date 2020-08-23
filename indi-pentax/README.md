# Pentax Camera Driver for Indi

## Introduction

This driver supports various Pentax cameras in PTP and/or MSC mode.  

## Installation

The driver requires two libraries: libRicohCameraSDKCpp and libPkTriggerCord.  These are included with indi-pentax in the indi-3rdparty repository.

```
sudo apt install indi-pentax
```

## Building

To build/install everything manually, first clone the indi-3rdparty repository:

```
mkdir -p ~/Projects
cd ~/Projects
git clone https://github.com/indilib/indi-3rdparty.git
```

Next install libRicohCameraSDK (skip for ARM64):

```
mkdir -p ~/Projects/build/libricohcamerasdk
cd ~/Projects/build/libricohcamerasdk
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty/libricohcamerasdk/
make
sudo make install
```

Next build/install libPkTriggerCord:

```
mkdir -p ~/Projects/build/libpktriggercord
cd ~/Projects/build/libpktriggercord
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty/libpktriggercord/
make
sudo make install
```

Finally, build/install indi-pentax:

```
mkdir -p ~/Projects/build/indi-pentax
cd ~/Projects/build/indi-pentax
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi-3rdparty/indi-pentax/
make
sudo make install
```

## Compatibility

In general, a greater number of cameras are supported in MSC mode.  However, in certain use cases on more recent cameras (e.g. no bulb mode, no prime focus), PTP mode is more reliable and will get you live view as well.  See known issues.

Based on the documentation for the libraries that this driver relies upon, the following cameras *should* work.  However, only cameras with an asterisk are actually confirmed.  Please update this list if you verify support for any camera:

- Pentax K-01 (MSC - known bugs)
- PENTAX K-1 (PTP, MSC?)
- PENTAX K-1 Mark II (PTP, MSC?)
- Pentax K-3 / K-3 II (MSC)
- Pentax K-5 (MSC)
- Pentax K-5 II / K-5 IIs (MSC)
- Pentax K-7 (MSC)
- Pentax K10D / Samsung GX-10 (MSC - fw 1.20 or later)
- Pentax K20D / Samsung GX-20 (MSC)
- Pentax K-30 (MSC - no bulb) *
- Pentax K-50 (MSC - known bugs)
- PENTAX K-70 (PTP, MSC - with bugs) *
- Pentax K200D (MSC)
- Pentax K-500 (MSC)
- PENTAX 645Z (PTP, MSC?)
- Pentax K-r (MSC)
- Pentax K-m / K2000 (MSC)
- PENTAX KP (PTP, MSC?)

Cameras suspected to have limited MSC support include:

- Pentax istDS2
- Pentax istDL
- Pentax K100D Super

Cameras likely *not* to work include:

- Pentax istDS
- Pentax istD
- Samsung GX-1L
- Pentax K110D
- Pentax K100D
- Pentax K-S2


## Features

The exact set of features available will depend on the current USB mode and capture mode of the camera.  Not all features will be available on all cameras.  The following is a rough list of what to expect:

- Still image capture 
- Live View (PTP mode only)
- Capture as FITS (processor intensive), Native, or both
- Change image format (JPEG, PEF, or DNG) (DNG is saved as a .raw file)
- Predefined capture mode support (e.g. Auto, Manual, etc.)
- Bulb mode support (MSC mode only)
- Set shutter speed to any supported by the capture mode to which the camera is currently set
- Change ISO (For certain cameras, such as K70, works in PTP mode only)
- Change Exposure (For certain cameras, such as K70, works in PTP mode only)
- Change White Balance
- Change JPEG image quality 
- Change JPEG image resolution (PTP mode only)
- Toggle save to SD Card (PTP mode only)
- Monitor battery level

The driver *should* support multiple cameras at once, and the author is happy to verify that if anyone wants to donate another camera.  

*If there's a feature that PkTriggerCord supports for your camera, but the driver currently does not, it should be possible to add support.  Contact the author with requests.*

## Operation

1. First, be sure the camera is in the desired USB mode (PTP or MSC).  Then connect the camera via a USB cable to the Indi host and power the camera on.
2. Set the camera to the appropriate capture mode.  For MSC mode, Bulb (B) provides maximum flexibility for exposures greater than 1 second, and M mode is recommended otherwise.  For PTP mode, Manual (M) is suggested for maximum flexibility.    
3. Start indiserver on the host with "Pentax DSLR (Native)" selected as a driver.  

If you are starting indiserver from the command line, you can use:

```
indiserver indi_pentax
```

4. Launch your Indi client, if you are not already in the client, and click "Connect" if you are not already connected.
5. Once connected, you should see a device pane for your Pentax camera.  The device pane will have a number of tabs.  You may change most settings in the Image Settings tab of the Indi Control Panel, though FITs/Native settings are in the Options tab.  
6. To capture an image using the Indi Control Panel, go to the Main Control tab and select an exposure duration.  Then click "Set" to start the exposure.  

*Note that unless you are in bulb mode, the exposure time you choose will not be the exact exposure time, but will be matched to the closest predefined exposure time of your current capture mode.*  

7. For Live View, be sure you are in PTP mode, and select the Streaming tab.

Images and Live View are also supported through Ekos, as explained in the Ekos documentation.

### Switching operational modes

You may switch capture modes (e.g. switch from Auto to Manual or Manual to Bulb) at any time.  

To switch between PTP and MSC, you will need to unplug the camera from the host and change to the desired USB mode using the on-camera menu.  You may need to manually disconnect from the driver in the Indi client (e.g. using the "Disconnect" button) if you are in MSC mode.  Then, plug the camera back into the host and click on "Connect" again.  A separate device pane will be created for your new USB mode, if it does not already exist.  Switch to the new device pane and cick "Connect" to continue in the new USB mode.

## Known Issues

### All modes
- On low-power systems (e.g. Raspberry Pi 3 and older), for performance reasons, the Native output format is recommended for the driver instead of FITS.  In such environments, *if you need FITs, I would recommend running KStars remotely, and configuring Ekos to auto-convert to FITS.*
- When DNG format is selected, images are currently saved with a "raw" extension.  This is because Indi seems to have a bug with .DNG files where it grabs the JPEG preview out of DNGs and discards the rest of the DNG file.  The raw files may be safely renamed to ".DNG."
- Download time estimates computed by Ekos are wrong, especially if you're using a slower system like a Raspberry Pi 3.

### PTP mode only
- No support for PTP mode on 64-bit ARM-based operating systems.
- Bulb capture does not work in PTP mode.
- Many settings (including exposure time) are unavailable in PTP mode when a lens is not attached (i.e. for prime focus).
- When compiled on a Raspberry Pi 3B running Ubuntu Mate 18.0.4 (32 bit), PTP mode does not work.  This appears to be because the indi_pentax binary generated by the compiler is for armv7, whereas the library files provided by Ricoh are for armv6.  Yet, I currently cannot figure out how to get indi-pentax to compile if I force the compiler to armv6.  A workaraound is to use a binary generated on Raspbian.

### MSC mode only
- No Live View.
- Changing ISO and exposure compensation do not work on the K-70 in MSC mode (probably other cameras as well).
- Driver-based conversion from JPEG to FITs throws a corrupt JPEG error.  This does not actually seem to cause problems, but I could be wrong.  To avoid the issue, though, select Native format for the driver, and configure KStars to convert to FITs if you need it (recommended anyway, see above).  Or capture in RAW format instead of JPEG.


## Author / Acknowledgments

This driver was developed by Karl Rees, copyright 2020.  

Thanks to Andras Salamon for PkTrigercord, which is used for MSC mode.  Specifically, this driver wraps PkTriggerCord into a shared library called libPkTriggerCord.  More information about PkTriggerCord and source code available from: https://api.ricoh/products/camera-sdk/.

Thanks also to Ricoh Company, Ltd. for providing the Ricoh Camera SDK (libRicohCameraSDKCpp), which is used for PTP mode.  The SDK is available from: https://api.ricoh/products/camera-sdk/.
