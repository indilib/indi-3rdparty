#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for migrating the RRD file from meteoweb to the structure used
# in weather radio.
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
import argparse
import rrdtool
import os.path
from os import path

parser = argparse.ArgumentParser(description="Migrate the RRD file from meteoweb to the structure used in weather radio")
parser.add_argument("infile", default="weather.rrd",
                    help="meteoweb RRD file")
parser.add_argument("outfile", default="weather.rrd",
                    help="target RRD file")

args=parser.parse_args()

# 5min raw values for 24 hours, 15 min for 7*24 hours, 1hour for 1 year,
# 1day dor 10 years.
ret = rrdtool.create(args.outfile, "-r", args.infile, "--step", "300",
                     "DS:Temperature=Temperature[1]:GAUGE:600:U:U",
                     # pressure data ignored since we change to sea level
		     "DS:Pressure=Pressure[1]:GAUGE:600:U:U",
                     "DS:Humidity=Humidity[1]:GAUGE:600:U:U",
                     "DS:CloudCover=CloudCover[1]:GAUGE:600:U:U",
                     "DS:SkyTemperature=SkyTemperature[1]:GAUGE:600:U:U",
                     "DS:DewPoint=DewPoint[1]:GAUGE:600:U:U",
                     "DS:SQM=SQM[1]:GAUGE:600:U:U",
		     "DS:WindSpeed=WindSpeed[1]:GAUGE:600:U:U",
		     "DS:WindGust:GAUGE:600:U:U",
		     "DS:WindDirection=WindDirection[1]:GAUGE:600:U:U",
                     "RRA:AVERAGE:0.5:1:288",
                     "RRA:AVERAGE:0.5:3:672",
                     "RRA:AVERAGE:0.5:12:8760",
                     "RRA:AVERAGE:0.5:288:3650",
                     "RRA:MIN:0.5:1:288",
                     "RRA:MIN:0.5:3:672",
                     "RRA:MIN:0.5:12:8760",
                     "RRA:MIN:0.5:288:3650",
                     "RRA:MAX:0.5:1:288",
                     "RRA:MAX:0.5:5:672",
                     "RRA:MAX:0.5:12:8760",
                     "RRA:MAX:0.5:288:3650")


if ret:
    print rrdtool.error()

