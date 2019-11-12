#!/usr/bin/python
# coding: utf-8

import sys
from munin_meteoweb import *

# ===========================================================================
# Munin Plugin for a meteoweb INDI server - humidity
# ===========================================================================

is_config = len(sys.argv) == 2 and sys.argv[1] == "config"

if is_config:
    print "graph_title Meteoweb Humidity"
    print 'graph_vlabel (%)'
    print "graph_category sensors"
    print "humidity.label Humidity (DHT22)"
else:
    readData()
    print "humidity.value %.4f" % sensorData['HR']

