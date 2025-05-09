#if 0
  This file is part of the AAG Cloud Watcher INDI Driver.
  A driver for the AAG Cloud Watcher (AAGware - http://www.aagware.eu/)

  Copyright (C) 2012-2015 Sergio Alonso (zerjioi@ugr.es)
  Copyright (C) 2019 Adrián Pardini - Universidad Nacional de La Plata (github@tangopardo.com.ar)



  AAG Cloud Watcher INDI Driver is free software: you can redistribute it
  and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  AAG Cloud Watcher INDI Driver is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AAG Cloud Watcher INDI Driver.  If not, see
  <http://www.gnu.org/licenses/>.
  
  Anemometer code contributed by Joao Bento.
#endif

#include "indicom.h"
#include "indiweather.h"
#include "CloudWatcherController_ng.h"

#include <iostream>

/**
 * Just a test main function. Used for debugging. Ignore it.
 */
int main(int /*argc*/, char ** /*argv*/)
{
    int PortFD = 0;
    int r = tty_connect("/dev/ttyUSB0", 9600, 8, 0, 1, &PortFD);

    if (r != TTY_OK)
    {
        std::cout << "Can't open serial port\n";
        return -1;
    }

    CloudWatcherController *cwc = new CloudWatcherController(false);

    cwc->setPortFD(PortFD);

    int check = cwc->checkCloudWatcher();

    if (check)
    {
        std::cout << "Cloudwatcher present\n";
    }
    else
    {
        std::cout << "Cloudwatcher NOT present\n";
        return -2;
    }

    CloudWatcherConstants constants;

    check = cwc->getConstants(&constants);

    if (check)
    {
        std::cout << "Firmware Version: " << constants.firmwareVersion << "\n";
        std::cout << "Serial Number: " << constants.internalSerialNumber << "\n";
        std::cout << "Zener Voltage: " << constants.zenerVoltage << "\n";
        std::cout << "LDR Max Resistance: " << constants.ldrMaxResistance << "\n";
        std::cout << "LDR PullUp Resistance: " << constants.ldrPullUpResistance << "\n";
        std::cout << "Rain Beta Factor: " << constants.rainBetaFactor << "\n";
        std::cout << "Rain Resistance At 25º: " << constants.rainResistanceAt25 << "\n";
        std::cout << "Rain PullUp Resistance: " << constants.rainPullUpResistance << "\n";
        std::cout << "Ambient Beta Factor: " << constants.ambientBetaFactor << "\n";
        std::cout << "Ambient Resistance At 25º: " << constants.ambientResistanceAt25 << "\n";
        std::cout << "Ambient PullUp Resistance: " << constants.ambientPullUpResistance << "\n";
        std::cout << "Anemometer Status: " << constants.anemometerStatus << "\n";
    }
    else
    {
        std::cout << "Problem getting constants\n";
        return -4;
    }

    CloudWatcherData cwd;

    check = cwc->getAllData(&cwd);

    if (check)
    {
        std::cout << "Supply: " << cwd.supply << "\n";
        std::cout << "Sky: " << cwd.sky << "\n";
        std::cout << "Sensor: " << cwd.sensor << "\n";
        std::cout << "TempEstimate: " << cwd.tempEst << "\n";
        std::cout << "TempActual: " << cwd.tempAct << "\n";
        std::cout << "Rain: " << cwd.rain << "\n";
        std::cout << "Rain Heater: " << cwd.rainHeater << "\n";
        std::cout << "Rain Temperature: " << cwd.rainTemperature << "\n";
        std::cout << "LDR: " << cwd.ldr << "\n";
        std::cout << "Light Freq: " << cwd.lightFreq << "\n";
        std::cout << "Read Cycle: " << cwd.readCycle << "\n";
        std::cout << "Wind Speed: " << cwd.windSpeed << "\n";
        std::cout << "Humidity: " << cwd.humidity << "\n";
        std::cout << "Pressure: " << cwd.pressure << "\n";

        std::cout << "Total Readings: " << cwd.totalReadings << "\n";

        std::cout << "Internal Errors: " << cwd.internalErrors << "\n";
        std::cout << "First Byte Errors: " << cwd.firstByteErrors << "\n";
        std::cout << "Second Byte Errors: " << cwd.secondByteErrors << "\n";
        std::cout << "PEC Byte Errors: " << cwd.pecByteErrors << "\n";
        std::cout << "Command Byte Errors: " << cwd.commandByteErrors << "\n";
    }
    else
    {
        std::cout << "Problem getting data\n";
        return -8;
    }

    delete cwc;

    return 0;
}
