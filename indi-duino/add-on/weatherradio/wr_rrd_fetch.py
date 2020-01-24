#!/usr/bin/python

import sys
import json
import rrdtool

rrdextract = rrdtool.fetch ("meteo.rrd", "AVERAGE", "-s", "-3h", "-r", "1min")

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
        series[categories[pos]]["data"].append({"x": time, "y": values[pos]})
    time += step

print json.dumps(series, indent=2, sort_keys=True)



