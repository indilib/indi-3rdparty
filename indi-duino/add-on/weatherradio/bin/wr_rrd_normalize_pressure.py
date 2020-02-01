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
import math
import argparse
import json
import rrdtool

from wr_config import *

def migrate(file, step):

    start = str(rrdtool.first(file))
    end   = str(rrdtool.last(file))

    rrdextract = rrdtool.fetch (file, "AVERAGE", "-s", start, "-e", end,
                                "-r", step)

    # first line: timeline
    [start, end, step] = rrdextract[0]
    # second line: categories
    categories = rrdextract[1]

    # search the pressure category
    i = 0
    pos = -1
    for cat in categories:
        if cat == "P":
            pos = i
        i+=1

    if pos == -1:
        print "RRD file does not contain pressure data"
        quit()

    # create a separate RRD file for normalized pressure
    ret = rrdtool.create("normalized.rrd", "--step", "60", "--start", '0',
		         "DS:P:GAUGE:600:U:U",
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
        print ret
        quit()

    lines = rrdextract[2]
    series = []

    #extract pressure data and calculate the normalized pressure
    factor = math.exp(ALTITUDE / 8000.0)
    t = start
    i = 0

    for values in lines:
        p = values[pos]

        if isinstance(p, float):
            p_sl = float(p) * factor
            rrdtool.update("normalized.rrd", "-t", "P", str(t) + ":" + str(p_sl))
            i+=1

        t += step


def main():
    parser = argparse.ArgumentParser(description="Migrate pressure in the data source to sea level normalized")
    parser.add_argument("rrdfile",
                        help="RRD file holding all time series")

    args=parser.parse_args()

    migrate(args.rrdfile, "60");


if __name__ == '__main__':
    main()
