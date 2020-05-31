#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for shooting single images with automatic exposure adaption
# day and night..
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

import os
from datetime import datetime
from pathlib import Path
import ConfigParser
from autoexposure import *

inifile_name = os.path.dirname(os.path.realpath(__file__)) + '/camera.ini'

def init_config():
    config = ConfigParser.ConfigParser()
    config.optionxform = str
    # default values
    config.add_section('Camera')
    config.set('Camera', 'ExposureTime', '400') # 1/250 sec
    config.set('Camera', 'BaseDirectory', ".")
    config.set('Camera', 'ISOSpeedRatings', '50')
    config.set('Camera', 'Contrast', '0')
    config.set('Camera', 'Brightness', '50')
    config.set('Camera', 'Saturation', '0')
    config.set('Camera', 'Options', '-md 4 -ex fixedfps')
    # night default settings
    config.add_section('Night')
    config.set('Night', 'Contrast', '100')
    config.set('Night', 'Brightness', '20')
    config.set('Night', 'Saturation', '-80')
    config.set('Night', 'MaxExposure', '10000000')
    config.set('Night', 'MaxISO', '800')
    config.read(inifile_name)
    return config
    
now = datetime.now()
config = init_config()

dir = config.get('Camera', 'BaseDirectory') + '/' +  now.strftime("%Y-%m-%d")

filename   = dir + "/" + now.strftime("%Y-%m-%d_%H%M%S") + ".jpg"
exptime    = config.getint('Camera', 'ExposureTime')
iso        = config.getint('Camera', 'ISOSpeedRatings')
brightness = config.getint('Camera', 'Brightness')
contrast   = config.getint('Camera', 'Contrast')
saturation = config.getint('Camera', 'Saturation')
opts       = config.get('Camera', 'Options')

# ensure that the image directory exists
if not Path(dir).exists():
    Path(dir).mkdir(parents=True)

# change to auto exposure for short exposure times
expstr = "-ss %d" % (exptime) if exptime > 10000 else "-ex auto"

# shoot the image
os.system("raspistill %s -ISO %d -br %d -co %d -sa %d %s -o %s"  % (opts, iso, brightness, contrast, saturation, expstr, filename))

# calculate the optimal exposure time
(realExpTime, brightness) = calibrateExpTime(filename, config)


configfile = open(inifile_name, 'w')
config.write(configfile)
configfile.close()

print "date=%s; time=%s; file=%s; ex=%d; iso=%d; bright=%s" % (now.strftime("%Y-%m-%d"), now.strftime("%H:%M:%S"), filename, realExpTime, iso, brightness)

#opts = "-awb greyworld -md 4 -ss 6000000 -ISO 800 -br 80 -co 100 -ex fixedfps -t 21000000 -tl 30000"
#opts = "-q 100 -md 4 -ss 10000000 -ISO 200 -ex fixedfps"

