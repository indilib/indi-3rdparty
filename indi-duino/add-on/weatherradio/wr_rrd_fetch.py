#!/usr/bin/python

import sys
import json
import rrdtool

# time zone
timezone = 1

rrdextract = rrdtool.fetch ("meteo.rrd", "AVERAGE", "-s", "-3h", "-r", "5min")

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
    for pos in range(0, len(categories)-1):
        y = values[pos]
        if isinstance(y, float):
            series[categories[pos]]["data"].append([(time + timezone*3600)*1000, round(y, 2)])
    time += step

print json.dumps(series, indent=2, separators=(',', ':'), sort_keys=True)



