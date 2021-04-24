# Troubleshooting

## Safety Precautions:
- Set up the mount in accordance to the manual
    - make especially sure to align the triangle arrow marks to face each other, its the "home" position of the mount and assumes this position always prior to starting.
- Always make sure you are able power off the device without touching it, to avoid injuries
    - pull the power plug rather than flip the power switch

## USB Troubleshooting

### Wrong device node
The driver is not able to determine whether its connected to the correct serial device prior to communication being successfully established.
However the driver issues a log message to the user, if it is not able to communicate with the mount. Furthermore the device enum on linux devices may shuffle the device nodes around.
To fix this you can order udev to create a custom symlink to the device node by createing a new rule file.
Use `lsusb` do retrieve the identifier of your usb to serial adapter:

![Get product and vendor id](get-usb-product-vendor-id.png?raw=true)

> sudo nano /etc/udev/rules.d/10-local.rules

add:
<code>ACTION=="add", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="my_uart"</code>

change `ATTRS` values accordingly. Change the `SYMLINK` value to a name fitting your needs.

Use `CTRL`+`O` to save the file and `CTRL`+`x` to close the editor.

After unplugging and plugging in the adapter a new device node in `dev` is created with the name chosen.

You can change the serial name in the EKO configuration dialog.

**Caveate:** Since some manufacturers do not bother to name their devices properly, and do not request a proper vendor id this solution only works as long as you have not several devices sharing the same vendor and product ids. 

### Permission problems
Some linux distributions block users from using serial adapters which are not in a specific group. For astroberry it is called `dialout`.
Use `groups` to find out what groups your user is in:

![dmesg output example](get-groups-list.png?raw=true)

if your user does not have the `dialout` group in this list you may add it using:

> sudo gpasswd --add ${USER} dialout

The device permission issue should now be resolved after the next login.

### Bugreport: Provide Useful Debug Information
Please use the bug tracker in the connection tab:
![Open Context Menu](repository-url.png?raw=true)

Unfortunately this happends for several reasons, and complaining that it crashed really does not help alot.
Therefore if you encounter a crash, it is important to isolate the problem.
The Indi-Service is a complex system, to reduce side effects start the driver as an isolated instance. Navigate to the build directory you created on driver installation using your favorite file manager. Open a terminal in your build directory of the driver.

![Open Context Menu](start-local-instance-1.png?raw=true)

Then run the following command:

> indiserver -v ./indi_bresserexos2

The terminal should now block and display a lot of text lines:

![Terminal Output](start-local-instance-2.png?raw=true)

If the driver crashes on connection you need to connect a client, its recommended to create a new "debug" setup in eg. kstars, and connect to the test service locally not remotely only! E.g. Use the VNC connection to your Pi and create a new debug profile in EKOs:

![Local Profile](start-local-instance-3.png?raw=true)

Save and start the profile as usual, to see what happends.
If the driver crashes the EKOs application should notify the user with a error message, please also provide this message! Also the terminal from earlier should now contain some information. Copy and paste this information into a logfile, and provide it with your bug report.

If you are done without being able to reproduce the problem, chances are high, you have encounter side effects, with your equipment. You can stop the process in the terminal using the keyshortcut `CTRL`+`C`.

Also the application log in the EKOs software does contain useful information so copy and paste that into a file and provide that too, if possible.

![Application Log](start-local-instance-4.png?raw=true)

In this situation you may have encountered a side effect with your equipment. Using simple exclusion, should be help isolating the combination causing the problem. Simple start your setup with the mount and simulators only, and iteratively add components until it crashes.

Generally speaking, its not simple to reproduce issues with no information. With providing accurate descriptions on how the reproduce a problem you'll do your part in inproving the overall stability of the driver.

You may redact your location information, these should not matter. However you can test this by simply changing the location in EKOs.

### Missing Symbols
There were reported cases where, after updating the software packages of you linux distribution, you encounter crashes of the driver. 
If you set up a debug set up in the manner mentioned above, you can look through the output lines of the terminal. If encounter lines with something like:

> 1970-01-00T03:13:37: Driver ./indi_bresserexos2: : **symbol lookup error: undefined symbol**: _ZN4INDI10BaseDevice13getDeviceNameEv

it is likely you have this exact problem.
You can resolve this by simply recompiling the driver. 
Open your build directory in a terminal, similarly to the procedure in the debug information section.
Go to the [Driver installation](Installation.md) documentation to the stage where you perform the `cmake --build .` command and simply rerun the process from there.

However there may cases where this does not work. So if you encounter linker or compiler error in the process, create a bug ticket, with keep the "useful debug information"-paradigm in mind.
