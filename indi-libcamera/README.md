This is the INDI driver for the libcamera-based cameras. It was tested
with IMX219. It is still under heavy development and not ready for production.

COMPILING

Go to the directory where  you unpacked indi_asi sources and do:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
```

should build the indi_libcamera_ccd executable.

RUNNING

The Driver can run multiple devices if required, to run the driver:

`indiserver -v indi_libcamera_ccd`

AVAILABLE CONTROLS

TODO 

You can also start video stream.

NOTES

Still under development.
