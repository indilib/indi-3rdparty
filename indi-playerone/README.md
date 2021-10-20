This is the INDI driver for the Player One Astronomy's cameras. It was tested
with the Neptune-C II but, hopefully, should work with
other cameras from Player One Astronomy too.

COMPILING

Go to the directory where  you unpacked indi_playerone sources and do:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
```

should build the indi_playerone_ccd executable.

RUNNING

The Driver can run multiple devices if required, to run the driver:

`indiserver -v indi_playerone_ccd`

AVAILABLE CONTROLS

You can set the exposure time and the gain (in the range between 1 and
100) You can also change the USB Bandwidth control. It defaults to -1
which mean auto. If you have problems with overload USB bus try a
value between 40 and 80. For example on ARM systems it's advised to
set a value of 40 if you find problems with many broken frames.

You can also start video stream.

TESTING

The driver was tested with KStars/EKOS as a remote INDI
server (just select remote driver, the IP of machine where indi_playerone_ccd
is running and the default port 7624). Connect to the camera you want
to use and have fun!

NOTES

The PlayerOne Cameras are very USB bandwidth hungry when running at high
FPS. If you see "broken frames" with more that one of them running,
keep in mind this important aspect.

PlayerOne Cameras continuously generate frames and they are buffered in the
driver. To avoid having frames with old parameters when these are
changed, a flushing mechanism is employed. It looks OK, but if you
find any problem with parameters changes not being immediately applied
please report.

CREDITS

The origianl INDI driver was written by Chrstian Pellegrin <chripell@gmail.com> based on ASI SDK v1.0+

The driver was completely rewritten by Jasem Mutlaq <mutlaqja@ikarustech.com> based on ASI SDK v2.0+

The driver was modified for PlayerOne cameras by Hiroshi Saito <hiro3110g@gmail.com> based on indi_asi_ccd
