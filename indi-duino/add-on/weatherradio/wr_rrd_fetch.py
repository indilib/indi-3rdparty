#!/usr/bin/python

import sys
import json
import rrdtool

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
            series[categories[pos]]["data"].append({"x": time, "y": round(y, 2)})
    time += step

print json.dumps(series, indent=2, sort_keys=True)



