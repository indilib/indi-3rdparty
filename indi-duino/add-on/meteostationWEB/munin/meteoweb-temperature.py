#!/usr/bin/python
# coding: utf-8

import sys
from munin_meteoweb import *

# ===========================================================================
# Munin Plugin for a meteoweb INDI server - Temperature
# ===========================================================================

is_config = len(sys.argv) == 2 and sys.argv[1] == "config"

if is_config:
    print "graph_title Meteoweb Temperature"
    print "graph_vlabel °C"
    print "graph_category sensors"
    print "temp_t.label Temperature (MLX90614)"
    print "temp_ir.label IR Temperature (MLX90614)"
    print "temp_p.label Temperature (BMP085)"
    print "temp_dht.label Temperature (DHT22)"
else:
    readData()
    print "temp_t.value %.4f" % sensorData['Tir']
    print "temp_ir.value %.4f" % sensorData['IR']
    print "temp_p.value %.4f" % sensorData['Tp']
    print "temp_dht.value %.4f" % sensorData['Thr']
