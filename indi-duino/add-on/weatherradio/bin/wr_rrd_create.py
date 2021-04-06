#!/usr/bin/python3

#-----------------------------------------------------------------------
# Script for creating the RRD file used to store the weather radio
# time series.
#
# Copyright (C) 2020-21 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------


import sys
import argparse
import rrdtool
from wr_config import *

parser = argparse.ArgumentParser(description="Create the RRD file storage for weather radio time series")
parser.add_argument("rrdfile", nargs='?', default=RRDFILE,
                    help="RRD file holding all time series")
parser.add_argument("-s", "--source",
                    help="Source file holding already captured data")

args=parser.parse_args()

#from meteoconfig import *

rrd_args = [args.rrdfile, "--step", "60"]

if (args.source):
    rrd_args += ["--source", args.source]

# 1min raw values for 1h, 5 min fpr 24 hours, 15 min for 7*24 hours,
# 1hour for 1 year, 1day dor 10 years.
ret = rrdtool.create(rrd_args,
		     "DS:Temperature:GAUGE:600:U:U",
		     "DS:Pressure:GAUGE:600:U:U",
		     "DS:Humidity:GAUGE:600:U:U",
		     "DS:DewPoint:GAUGE:600:U:U",
		     "DS:CloudCover:GAUGE:600:U:U",
		     "DS:SkyTemperature:GAUGE:600:U:U",
		     "DS:SQM:GAUGE:600:U:U",
		     "DS:WindSpeed:GAUGE:600:U:U",
		     "DS:WindGust:GAUGE:600:U:U",
		     "DS:WindDirection:GAUGE:600:U:U",
		     "DS:RaindropFrequency:GAUGE:600:U:U",
		     "DS:RainVolume:GAUGE:600:U:U",
		     "RRA:AVERAGE:0.5:1m:1h",
		     "RRA:AVERAGE:0.5:5m:1d",
		     "RRA:AVERAGE:0.5:15m:7d",
		     "RRA:AVERAGE:0.5:1h:1y",
		     "RRA:AVERAGE:0.5:1d:10y",
		     "RRA:MIN:0.5:1m:1h",
		     "RRA:MIN:0.5:5m:1d",
		     "RRA:MIN:0.5:15m:7d",
		     "RRA:MIN:0.5:1h:1y",
		     "RRA:MIN:0.5:1d:10y",
		     "RRA:MAX:0.5:1m:1h",
		     "RRA:MAX:0.5:5m:1d",
		     "RRA:MAX:0.5:15m:7d",
		     "RRA:MAX:0.5:1h:1y",
		     "RRA:MAX:0.5:1d:10y")


if ret:
		print(rrdtool.error())

