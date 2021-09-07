## Install / Update / Removal

### Prerequisites
Make sure you have software in this section installed before the build attempt.

Run a software update to get the latest version of any package for your distribution:

> `sudo apt-get update`

and

> `sudo apt-get dist-upgrade`

after conclusion, restart your sytem.

#### Libindi
Check if you have the libindi software suite installed:

>``sudo apt-get install libindi1 libindidriver1 libindi-data libindi-dev libindi-plugins libindialignmentdriver1 libnova-0.16-0 libnova-dev libnova-dev-bin``

This should be done on Astroberry, but may be necessary on a different distribution.

**Notes:** `libnova` may have a different version number on your distribution, if you can not find this particular version use a `apt-cache search libnova` to find the correct name to use.

#### Development Tools and Build System
Please check if cmake is installed, since its the primary build system and throw in a build-essential for good measure.

>``sudo apt-get install cmake build-essential git``

Apart from cmake, this should already be installed on Astroberry, but may be necessary on a different distribution.

Wait until everything is installed, and continue with building the driver.

### Building and Installing the Driver
1. Clone the Repository into a directory of the Pi (eg. your Home directory):
> ``git clone https://github.com/kneo/indi-bresserexos2.git``

**Note** Make sure you are allowed to write to the current directory!

2. Change Directory to BresserExosIIDriverForIndi:
> ``cd indi-bresserexos2``

3. Create a directory "build" in the current directory:
> ``mkdir build``

4. Change to the build directory:
> ``cd build``

5. Run ``cmake ..`` (and wait for completion):
	- provide``CMAKE_INSTALL_PREFIX`` to adjust the install location of the driver binary if necessary.
	- provide ``XML_INSTALL_DIR`` to adjust the location of the xml file for indi if necessary.

6. Run build process (and wait for the conclusion):
> ``cmake --build .``

7. Install the driver (if everything seems working):
> ``sudo make install``

8. Restart the indi Service or simply restart the Pi
	- This is necessary for the driver to be visible in the Web Manager

9. Go To the Astroberry Manager Web Page:
	- http://ip-of-your-pi/desktop

10. Remain here do not "Connect" to the desktop, rather Go To INDI Webmanager:
	- Click the little scope on the side bar

11. Look throught the Driver list for "Exos II GoTo"
	- If it appears, the installation is completed.
	
### Update The Driver
Its recommended to keep the repository stored on your device for easy updating. 

1. Make sure the indi software does not run, it should not on a freshly rebooted system. Use:
``fuser /usr/bin/indi_bresserexos2``
if it returns a number a process is still "using" the file, 
kill this process using: ``kill -9 PID``, where **PID** is the number the ``fuser`` command returned.

2. open the root directory of the driver source code repository in a terminal.
	- The context menu of your prefered file manager may have an appropriate context menu entry.
3. unless you changed any of the contents of the source directory the command should download all remote changes/updates. To get the changes use:
``git pull``

4. go into the ``build`` directory you created in step 3. of the installation guide using:
``cd build``

5. again run the ``cmake ..`` command.

6. when concluded run the ``cmake --build .`` again to start the build process.

7. once successfully finished, run:
``sudo make install`` to replace your existing install files.

8. the update is done, continue using the driver. Although is test is recommended.
The ``/usr/share/indi/indi_bresserexos2.xml`` file should contain the version number of the latest version. Also the Driver should show the correct version number in the kstars EKOS panel of the driver.

### Remove the Driver
If you want to remove your driver, you can do so by just deleting the files installed by ``sudo make install``. The default installation paths are:
- ``/usr/bin/indi_bresserexos2``
- ``/usr/share/indi/indi_bresserexos2.xml``
If you changed the paths, by eg. providing a ``CMAKE_INSTALL_PREFIX`` you have to adjust you path accordingly.

Use a ``rm /usr/bin/indi_bresserexos2`` and ``rm /usr/share/indi/indi_bresserexos2.xml`` to delete the files.

If you want to find out if you removed everything or don't know what to delete, use the ``find / -name "indi_bresserexos2*" 2>/dev/null`` command to find any "indi_bresserexos2*" related file on your file system.
Check through the output for any remaining binary file installed on your system This command may also find them in your home directory, but these can be considered inactive.
