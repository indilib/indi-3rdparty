#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for creating the RRD file used to store the weather radio
# time series.
#
# Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------


import sys
import rrdtool
#from meteoconfig import *

# 1min raw values for 24 hours, 5 min for 7*24 hours, 1hour for 1 year,
# 1day dor 10 years.
ret = rrdtool.create("weather.rrd", "-r", "meteo.rrd", "--step", "60",
		     "DS:Temperature=T[1]:GAUGE:600:U:U",
		     "DS:Pressure=P[1]:GAUGE:600:U:U",
		     "DS:Humidity=HR[1]:GAUGE:600:U:U",
		     "DS:CloudCover=clouds[1]:GAUGE:600:U:U",
		     "DS:SkyTemperature=skyT[1]:GAUGE:600:U:U",
		     "DS:DewPoint=Thr[1]:GAUGE:600:U:U",
		     "DS:SQM=Light[1]:GAUGE:600:U:U",
		     "RRA:AVERAGE:0.5:1:1440",
		     "RRA:AVERAGE:0.5:5:2016",
		     "RRA:AVERAGE:0.5:3600:8760",
		     "RRA:AVERAGE:0.5:86400:3650",
		     "RRA:MIN:0.5:1:1440",
		     "RRA:MIN:0.5:5:2016",
		     "RRA:MIN:0.5:3600:8760",
		     "RRA:MIN:0.5:86400:3650",
		     "RRA:MAX:0.5:1:1440",
		     "RRA:MAX:0.5:5:2016",
                     "RRA:MAX:0.5:3600:8760",
		     "RRA:MAX:0.5:86400:3650")


if ret:
		print rrdtool.error()

