#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for creating the RRD file used to store the weather radio
# time series.
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
from wr_config import *

parser = argparse.ArgumentParser(description="Create the RRD file storage for weather radio raw sensors time series")
parser.add_argument("rrdfile", nargs='?', default=RRDSENSORSFILE,
                    help="RRD file holding all time series of the raw sensor data")

args=parser.parse_args()


# 1min raw values for 24 hours, 15 min for 7*24 hours, 1hour for 1 year,
# 1day dor 10 years.
ret = rrdtool.create(args.rrdfile, "--step", "60", "--start", '0',
		     "DS:BME280_Temp:GAUGE:600:U:U",
		     "DS:BME280_Pres:GAUGE:600:U:U",
		     "DS:BME280_Hum:GAUGE:600:U:U",
		     "DS:DHT_Temp:GAUGE:600:U:U",
		     "DS:DHT_Hum:GAUGE:600:U:U",
		     "DS:MLX90614_Tamb:GAUGE:600:U:U",
		     "DS:MLX90614_Tobj:GAUGE:600:U:U",
		     "DS:TSL2591_Lux:GAUGE:600:U:U",
		     "RRA:AVERAGE:0.5:1:1440",
		     "RRA:AVERAGE:0.5:15:672",
		     "RRA:AVERAGE:0.5:60:8760",
		     "RRA:AVERAGE:0.5:1440:3650",
		     "RRA:MIN:0.5:1:288",
		     "RRA:MIN:0.5:15:672",
		     "RRA:MIN:0.5:60:8760",
		     "RRA:MIN:0.5:1440:3650",
		     "RRA:MAX:0.5:1:288",
		     "RRA:MAX:0.5:15:672",
                     "RRA:MAX:0.5:60:8760",
		     "RRA:MAX:0.5:1440:3650")


if ret:
		print rrdtool.error()

