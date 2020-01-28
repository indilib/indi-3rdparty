# Pentax Camera Driver for Indi

## Introduction

This driver supports various Pentax cameras in PTP and/or MSC mode.  

### PTP Mode

PTP mode support is provided via the Ricoh Camera SDK (see https://api.ricoh/products/camera-sdk/).  The number of cameras supported are 
relatively few compared to MSC mode, but a greater number of settings are supported, as is Live View.  A significant drawback is that Bulb
mode is not supported, meaning exposures are limited to 30 seconds.

### MSC Mode

MSC mode support is provided via PkTriggerCord (see http://pktriggercord.melda.info/), which has been wrapped in a shared library.  A larger number of cameras is supported, but the supported features vary between cameras.  Bulb mode is supported.

## Installation

The driver requires two libraries: libRicohCameraSDKCpp and libPkTriggerCord.  These are included with indi-pentax in the indi-3rdparty repository.

To build/install everything manually, first clone the indi-3rdparty repository:

```
mkdir -p ~/Projects
cd ~/Projects
git clone https://github.com/karlrees/indi-3rdparty.git
```

Next install libRicohCameraSDK:

```
cd ~/Projects/indi-3rdparty/libricohcamerasdk
cmake -DCMAKE_INSTALL_PREFIX=/usr .
make
sudo make install
```

Next build/install libPkTriggerCord:

```
cd ~/Projects/indi-3rdparty/libpktriggercord
cmake -DCMAKE_INSTALL_PREFIX=/usr .
make
sudo make install
```

Finally, build/install indi-pentax:

```
cd ~/Projects/indi-3rdparty/indi-pentax
cmake -DCMAKE_INSTALL_PREFIX=/usr .
make
sudo make install
```

## Compatibility

### PTP Mode

Cameras listed as officially supported by the SDK (but not yet confirmed): 
- PENTAX K-1 Mark II
- PENTAX KP
- PENTAX K-1
- PENTAX 645Z

Additional cameras confirmed to work: 
- PENTAX K-70

Cameras confirmed not to work: 
- None so far, but it is expected that only more recent cameras will work.

### MSC Mode

Any camera listed as supported by PkTriggerMode (see https://pktriggercord.melda.info).  This should be most Pentax cameras.

## Features

The driver supports multiple cameras at once.  The exact set of features available will depend on the current USB mode and capture mode of the camera.

### PTP Mode

- Live View
- Still image capture in JPEG, PEF, or DNG (saved as a .raw file)
- Capture as FITS (processor intensive), Native, or both
- Set shutter speed to any supported by the capture mode to which the camera is currently set (No Bulb)
- Change ISO, Exposure, White Balance
- Change image quality and resolution
- Toggle save to SD Card
- Monitor battery level

### MSC Mode

- Still image capture in JPEG, PEF, or DNG (saved as a .raw file)
- Capture as FITS (processor intensive), Native, or both
- Change image quality
- Change ISO (not working on K-70)
- Change Exposure (not working on K-70)
- Change White Balance
- Set shutter speed to any supported by the capture mode to which the camera is currently set
- Bulb mode support
- Monitor battery level

## Operation

1. First, be sure the camera is in the desired USB mode (PTP or MSC).  Then connect the camera via a USB cable to the Indi host and power the camera on.
2. Set the camera to the appropriate capture mode.  For PTP mode, Manual (M) is suggested for maximum flexibility.  For MSC mode, Bulb (B) provides maximum flexibility.  
However, if your exposures are less than 30 seconds, other modes (e.g. Manual) are close to twice as fast at starting the exposure and returning the image.
3. Start the Indi server on the host with "Pentax DSLR (Native)" selected as the driver.  Click "Connect" if the driver does not auto-connect.
4. Once connected, you may change most settings in the Image Settings tab of the Indi Control Panel, though FITs/Native settings are in the Options tab.  
5. To capture an image using the Indi Control Panel, go to the Main Control tab and select an exposure duration.  Then click "Set" to start the exposure.  

Note that unless you are in bulb mode, the exposure time you choose will not be the exact exposure time, but will be matched to the closest predefined exposure time of your current capture mode.  

6. For Live View, be sure you are in PTP mode, and select the Streaming tab.

Images and Live View are also supported through Ekos, as explained in the Ekos documentation.

### Switching operational modes

You may switch capture modes (e.g. switch from Auto to Manual or Manual to Bulb) at any time.  

To switch between PTP and MSC, you will need to unplug the camera from the host and change to the desired USB mode using the on-camera menu.  You may need to manually disconnect the driver (e.g. using the "Disconnect" button) if you are in MSC mode.  Then, plug the camera back into the host and click on "Connect" again.  A separate device tab will be created for your new USB mode, 
if it does not already exist.  Select the new device and cick "Connect" to continue in the new USB mode.

## Known Issues

- Bulb mode does not work in PTP mode.
- Changing ISO and exposure do not work on the K-70 in MSC mode (probably other cameras as well).
- When DNG format is selected, images are currently saved with a "raw" extension.  This is because Indi seems to have a bug with .DNG files where it grab the JPEG
preview out of DNGs and discards the rest of the DNG file.  The raw files may be safely renamed to ".DNG."
- When compiled on Ubuntu Mate 18.0.4 (Raspberry Pi 3B), PTP mode does not work.  This appears to be because the binary generated by the compiler
is for armv7, whereas the library files provided by Ricoh are for armv6.  Yet, I currently cannot figure out how to get indi-pentax to compile if I force the compiler
to armv6.  A workaraound is to use a binary generated on Rasbian.
- The Ricoh Camera SDK uses a custom version of libmtp with a version number of 9.3.0.  Not surprisingly, this can cause problems if the
standard version of libmtp is installed.  To avoid these problems, I install the customized libmtp along with the other library files in the "indipentax"
subdirectory of CMAKE_INSTALL_LIBDIR, and then list that subdirectory in the RPATH of the indi_pentax binary (not the RUNPATH, since libmtp is an indirect dependency).
I'm sure this negatively affects the modularity of indi-pentax.  I'm happy to take suggestions if there's a better way to deal with this issue.

## AUTHOR / CONTRIBUTORS

This driver was developed by Karl Rees, copyright 2020.  

Thanks to Andras Salamon for producing PkTrigercord, and to Ricoh Company, Ltd. for providing the Ricoh Camera SDK.
