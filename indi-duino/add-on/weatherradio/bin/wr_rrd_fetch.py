#!/usr/bin/python

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
import json
import rrdtool

parser = argparse.ArgumentParser(description="Fetch time series from the RRD file as JSON document")
parser.add_argument("-s", "--start", default="1d",
                    help="interval starting time relative to now()")
parser.add_argument("-r", "--steps", default="5min",
                    help="distance between to data steps")
parser.add_argument("-t", "--timezone", default=1,
                    help="Timezone for which the data series has been collected")
parser.add_argument("rrdfile",
                    help="RRD file holding all time series")

args=parser.parse_args()
    
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

print json.dumps(series, indent=2, separators=(',', ':'), sort_keys=True)



