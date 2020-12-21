#!/usr/bin/python

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
from indiclient import *
from weatherradio import *
from os import path
import argparse

parser = argparse.ArgumentParser(description="Fetch weather data and store it into the RRD file")
parser.add_argument("-v", "--verbose", action='store_true',
                    help="Display progress information")
parser.add_argument("rrdfile", nargs='?', default=RRDFILE,
                    help="RRD file holding all time series")
parser.add_argument("rrdsensorsfile", nargs='?', default=RRDSENSORSFILE,
                    help="RRD file holding all sensor data time series")

args = parser.parse_args()
indi = None


try:
    if (args.verbose):
        print "Updating data from \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)

    # open connection to the INDI server
    indi=indiclient(INDISERVER,int(INDIPORT))

    # ensure that the INDI driver is connected to the device
    connect = connect(indi, verbose=args.verbose)

    if (connect):
        if (args.verbose):
            print "Connection established to \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)

        data = None
        if path.exists(args.rrdfile):
            data = readWeather(indi, verbose=args.verbose)
            updateRRD(args.rrdfile, data)

        data = None
        if path.exists(args.rrdsensorsfile):
            data = readSensors(indi)
            updateRRD(args.rrdsensorsfile, data)

        if (args.verbose):
            print "Weather parameters read from \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)
    else:
        print "Establishing connection FAILED to \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)


    indi.quit()

except:
    print "Updating data from \"%s\"@%s:%s FAILED!" % (INDIDEVICE,INDISERVER,INDIPORT)
    if indi != None:
        indi.quit()
    sys.exit()
