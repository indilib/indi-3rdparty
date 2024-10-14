INDI-3rd Party Driver Update Tool

About: 
The INDI Driver Update Tool helps keep Debian packages up-to-date with the latest drivers from the INDI third-party repository with the aim of keeping Debian packages current with the latest INDI drivers, making astronomical instrumentation control easier and more reliable.

Functions: 
- Finds available drivers in the repository.
- Checks existing Debian packages.
- Identifies packages needing updates.
- Prepares updated packages for upload.

Requirements: 
INDI >= V1.0
Python >= 3.6
...

Installation: 
See <<<INSTALL>>> for installation instructions.

Usage: 
To update your INDI-3rd party drivers, simply run the following command:
<<<command>>>

Configuration: 
You can customize the update process by editing the config.json file.
This file allows you to specify the drivers you want to update, as well as the update frequency.

Troubleshooting: 

Supported drivers: 
Currently, supported INDI-3rd party drivers include the following:
