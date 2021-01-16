#!/usr/bin/python3

#-----------------------------------------------------------------------
# Script for extracting the last update from the RRD file and export them
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
import datetime, time
import argparse
import simplejson as json
import rrdtool
from wr_config import *

parser = argparse.ArgumentParser(description="Fetch the last updat from the RRD file as JSON document")
parser.add_argument("-t", "--timezone", default=1, type=int,
                    help="Timezone for which the data series has been collected")
parser.add_argument("-o", "--output",
                    help="JSON file to be written")
parser.add_argument("rrdfile", nargs='*', default=RRDFILE,
                    help="RRD file holding all time series")

args = parser.parse_args()

# if not set, use default output file
if not args.output:
    args.output = DATAPATH + "/RTdata_lastupdate.json"


result = rrdtool.lastupdate (args.rrdfile)

# Result comes as dictionary with timestamp and the data as separate
# dictionary. For simplicity reasons, we merge it into one single dictionary

data = result['ds']
last = result['date']
data['timestamp'] = int(time.mktime(last.timetuple())*1000)

output = open(args.output, 'w')
output.write(json.dumps(data, indent=2, separators=(',', ':'), sort_keys=True, ignore_nan=True))
output.close()



