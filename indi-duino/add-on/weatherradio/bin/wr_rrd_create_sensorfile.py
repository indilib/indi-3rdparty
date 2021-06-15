#!/usr/bin/python3

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

# initialize the configuration
config = WeatherRadioConfig().config

parser = argparse.ArgumentParser(description="Create the RRD file storage for weather radio raw sensors time series")
parser.add_argument("rrdfile", nargs='?',
                    default=config.get('WeatherRadio', 'RRDSENSORSFILE'),
                    help="RRD file holding all time series of the raw sensor data")
parser.add_argument("-s", "--source",
                    help="Source file holding already captured data")

args=parser.parse_args()


rrd_args = [args.rrdfile, "--step", "300"]

if (args.source):
    rrd_args += ["--source", args.source]

# 5min raw values for 24 hours, 15 min for 7*24 hours, 1hour for 10 years.
ret = rrdtool.create(rrd_args,
		     "DS:BME280_Temp:GAUGE:600:U:U",
		     "DS:BME280_Pres:GAUGE:600:U:U",
		     "DS:BME280_Hum:GAUGE:600:U:U",
		     "DS:DHT_Temp:GAUGE:600:U:U",
		     "DS:DHT_Hum:GAUGE:600:U:U",
		     "DS:MLX90614_Tamb:GAUGE:600:U:U",
		     "DS:MLX90614_Tobj:GAUGE:600:U:U",
		     "DS:TSL2591_Lux:GAUGE:600:U:U",
		     "RRA:AVERAGE:0.5:5m:24h",
		     "RRA:AVERAGE:0.5:15m:7d",
		     "RRA:AVERAGE:0.5:1h:1y",
		     "RRA:AVERAGE:0.5:1d:10y",
		     "RRA:MIN:0.5:5m:24h",
		     "RRA:MIN:0.5:15m:7d",
		     "RRA:MIN:0.5:1h:1y",
		     "RRA:MIN:0.5:1d:10y",
		     "RRA:MAX:0.5:5m:24h",
		     "RRA:MAX:0.5:15m:7d",
		     "RRA:MAX:0.5:1h:1y",
		     "RRA:MAX:0.5:1d:10y")

if ret:
		print(rrdtool.error())

