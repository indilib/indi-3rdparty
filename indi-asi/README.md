This is the INDI driver for the ZWO Optics ASI cameras. It was tested
with the ASI120MC and the ASI120MM but, hopefully, should work with
other cameras from ZWO Optical too.

COMPILING

Go to the directory where  you unpacked indi_asi sources and do:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
```

should build the indi_asicam executable.

RUNNING

The Driver can run multiple devices if required, to run the driver:

`indiserver -v indi_asi_ccd`

AVAILABLE CONTROLS

You can set the exposure time and the gain (in the range between 1 and
100) You can also change the USB Bandwidth control. It defaults to -1
which mean auto. If you have problems with overload USB bus try a
value between 40 and 80. For example on ARM systems it's advised to
set a value of 40 if you find problems with many broken frames.

You can also start video stream.

TESTING

The driver was tested with KStars/EKOS as a remote INDI
server (just select remote driver, the IP of machine where indi_asicam
is running and the default port 7624). Connect to the camera you want
to use and have fun!

NOTES

The ASICameras are very USB bandwidth hungry when running at high
FPS. If you see "broken frames" with more that one of them running,
keep in mind this important aspect.

ASICameras continuously generate frames and they are buffered in the
driver. To avoid having frames with old parameters when these are
changed, a flushing mechanism is employed. It looks OK, but if you
find any problem with parameters changes not being immediately applied
please report.

NICKNAMES

The ASI SDK exposes device serial numbers for at least CCDs and EAFs.
You may associate nicknames with specific serial numbers to make, *e.g.*,
setups with multiple identical devices reliably associate the same name
with the same device across restarts.

Nicknames are stored in an xml-format file in a format like the below.
The hard-wired location for this file is ``~/.indi/ZWONicknames.xml``.
Nicknames are assoicated with the serial number of the camera, and are
entered/changed with the NicknameTP text property. Since the device-name
can't be changed once the driver is running, changes to nicknames can
only take effect at the next INDI startup.

You may mix CCD and EAF nicknames in the same file, using the format:

```
<?xml version="1.0"?>
<Nicknames>
  <Nickname SerialNumber="serialNumber1">nickname1</Nickname>
  <Nickname SerialNumber="serialNumber2">nickname2</Nickname>
  <Nickname SerialNumber="serialNumber3">nickname3</Nickname>
</Nicknames>
```

CREDITS

The origianl INDI driver was written by Chrstian Pellegrin <chripell@gmail.com> based on ASI SDK v1.0+

The driver was completely rewritten by Jasem Mutlaq <mutlaqja@ikarustech.com> based on ASI SDK v2.0+
