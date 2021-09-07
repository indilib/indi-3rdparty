#!/usr/bin/python3

#-----------------------------------------------------------------------
# Script for extracting time series from the RRD file and export them
# as JSON file.
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
import simplejson as json
import rrdtool
import time
from wr_config import *

# initialize the configuration
config   = WeatherRadioConfig().config

# calculate UTC offset considering DST
is_dst = time.daylight and time.localtime().tm_isdst > 0
utc_offset = - (time.altzone if is_dst else time.timezone) / 3600

# default step sizes
default = {}
default['steps'] = {}
default['steps']['6h'] = '5min'
default['steps']['1d'] = '15min'
default['steps']['7d'] = '1h'
default['steps']['30d'] = '1d'

parser = argparse.ArgumentParser(description="Fetch time series from the RRD file as JSON document")
parser.add_argument("-s", "--start", default='6h',
                    help="interval starting time relative to now()")
parser.add_argument("-r", "--steps",
                    help="distance between to data steps")
parser.add_argument("-t", "--timezone", default=utc_offset, type=int,
                    help="Timezone for which the data series has been collected")
parser.add_argument("-o", "--output",
                    help="JSON file to be written")
parser.add_argument("rrdfile", nargs='?',
                    default=config.get('WeatherRadio', 'RRDFILE'),
                    help="RRD file holding all time series")

args=parser.parse_args()

# use default step sizes
if not args.steps:
    if args.start in default['steps']:
        args.steps = default['steps'][args.start]
    else:
        args.steps = '5min'

# if not set, use default output file
if not args.output:
    args.output = config.get('WeatherRadio', 'DATAPATH') + "/RTdata_" + args.start + ".json"

rrdextract = rrdtool.fetch (args.rrdfile, "AVERAGE", "-s", "-" + args.start,
                            "-r", args.steps)

# first line: timeline
[start, end, step] = rrdextract[0]
# second line: categories
categories = rrdextract[1]

series = {}

# initialize the categories
for cat in categories:
    series[cat] = {"name": cat, "data": []}

lines = rrdextract[2]

#fill categories
time = start

for values in lines:
    for pos in range(0, len(categories)):
        y = values[pos]
        if isinstance(y, float):
            series[categories[pos]]["data"].append([(time + args.timezone*3600)*1000, round(y, 2)])
    time += step

output = open(args.output, 'w')
output.write(json.dumps(series, indent=2, separators=(',', ':'), sort_keys=True, ignore_nan=True))
output.close()



