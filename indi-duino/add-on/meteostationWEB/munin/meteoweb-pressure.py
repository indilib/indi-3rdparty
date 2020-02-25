#!/usr/bin/python
# coding: utf-8

import sys
from munin_meteoweb import *

# ===========================================================================
# Munin Plugin for a meteoweb INDI server - barometric pressure
# ===========================================================================

is_config = len(sys.argv) == 2 and sys.argv[1] == "config"

if is_config:
    print "graph_title Meteoweb Barometric Pressure"
    print "graph_vlabel hPa"
    print "graph_category sensors"
    print "pressure.label Barometric Pressure (BMP085)"
else:
    readData()
    print "pressure.value %.4f" % sensorData['P']

