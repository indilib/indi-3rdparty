This is the INDI driver for the libcamera-based cameras. It was tested
with IMX219. It is still under heavy development and not ready for production.

COMPILING

Install additional build dependency:

```
sudo apt-get install libboost-program-options1.74-dev
```

Go to the directory where  you unpacked indi-libcamera sources and do:

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

## Gain Conversion (HCG/LCG)

The Gain Conversion control is available for IMX290-family sensors when the
kernel exposes the following sysfs interface:

    /sys/module/imx290/parameters/hcg_mode

When this interface is available, indi-libcamera exposes a "Gain Conversion"
property that allows switching between:

- Dynamic Range (LCG)
- Low Noise (HCG)

If the sysfs interface is not present, the property is hidden.

By default, the sysfs interface is typically writable only by root.

One way to allow a non-root `indiserver` to control Gain Conversion is to
change the group ownership to a group that the `indiserver` user belongs to,
for example:

```bash
sudo chgrp video /sys/module/imx290/parameters/hcg_mode
```

For a permanent configuration, use a udev rule or another system-specific
mechanism to assign the desired group automatically.

TODO 

You can also start video stream.

NOTES

Still under development.
