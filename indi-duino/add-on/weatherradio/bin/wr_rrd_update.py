#!/usr/bin/python3

#-----------------------------------------------------------------------
# Script for updating the RRD time series.
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@t-online.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------

import sys
from weatherradio import *
from indiclient import *
from os import path
import argparse
from pid.decorator import pidfile
from pid import PidFileError

def getWeatherData(args, indi, indiserver, indiport, device):
    if (args.verbose):
        print ("Reading weather data from \"%s\"@%s:%s" %
               (device, indiserver, indiport))

    data = readWeather(indi, device, verbose=args.verbose)
    return data


def getSensorData(args, indi, indiserver, indiport, device):
    if (args.verbose):
        print ("Reading sensor data from \"%s\"@%s:%s" %
               (device, indiserver, indiport))

    data = readSensors(indi, device)
    return data

@pidfile()
def update(args, indiserver, indiport):
    try:
        # open connection to the INDI server
        indi=indiclient(indiserver, indiport)

        weatherData = {}
        sensorData  = {}

        # update data from all devices
        for deviceID in wrConfig.getDevices():
            device = config.get(deviceID, 'INDIDEVICE')

            # ensure that the INDI driver is connected to the device
            connected = connect(indi, deviceID, verbose=args.verbose)

            if (connected):
                weatherData.update(getWeatherData(args, indi, indiserver,
                                                  indiport, device))
                sensorData.update(getSensorData(args, indi, indiserver,
                                                indiport, device))
            else:
                print ("Establishing connection FAILED to \"%s\"@%s:%s" % (deviceID, indiserver, indiport))

        # update RRD files with all retrieved data
        if path.exists(args.rrdfile):
            updateRRD(args.rrdfile, weatherData)
        else:
            print ("Cannot store weather data, file %s not found." %
                   (args.rrdfile))

        if path.exists(args.rrdsensorsfile):
            updateRRD(args.rrdsensorsfile, sensorData)
        else:
            print ("Cannot store sensor data, file %s not found." %
                   (args.rrdsensorsfile))

        # finished
        indi.quit()
        sys.exit()

    except PidFileError:
        print ("RRD update still running")
        if indi != None:
            indi.quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Fetch weather data and store it into the RRD file")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="Display progress information")
    parser.add_argument("rrdfile", nargs='?',
                        default=config.get('WeatherRadio', 'RRDFILE'),
                        help="RRD file holding all time series")
    parser.add_argument("rrdsensorsfile", nargs='?',
                        default=config.get('WeatherRadio', 'RRDSENSORSFILE'),
                        help="RRD file holding all sensor data time series")

    args = parser.parse_args()

    try:
        update(args, config.get('WeatherRadio', 'INDISERVER'),
               config.getint('WeatherRadio', 'INDIPORT'))
    except Exception as ex:
        print("Problem occured: {0}".format(ex))
        sys.exit()
