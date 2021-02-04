#!/usr/bin/python3

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

import os, stat
from datetime import datetime
from pathlib import Path
from configparser import ConfigParser
from pid.decorator import pidfile
from pid import PidFileError
from autoexposure import *

def init_config(inifile_name):
    config = ConfigParser(interpolation=None)
    config.optionxform = str
    # default values
    config.add_section('Camera')
    config.set('Camera', 'ExposureTime', '400') # 1/250 sec
    config.set('Camera', 'BaseDirectory', ".")
    config.set('Camera', 'ConverterFIFO', "/tmp/imageconverter.fifo")
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
    
@pidfile()
def main():
    inifile_name = os.path.dirname(os.path.realpath(__file__)) + '/camera.ini'

    now = datetime.now()
    config = init_config(inifile_name)

    dir = config.get('Camera', 'BaseDirectory')

    filename   = now.strftime("%Y-%m-%d_%H%M%S") + ".jpg"
    fullname   = dir + '/' + filename
    fifo       = config.get('Camera', 'ConverterFIFO')
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
    os.system("raspistill %s -ISO %d -br %d -co %d -sa %d %s -o %s"  % (opts, iso, brightness, contrast, saturation, expstr, fullname))

    # calculate the optimal exposure time
    (imgExpTime, imgBrightness) = calibrateExpTime(fullname, config)

    # start converter process
    if os.path.exists(fifo) and stat.S_ISFIFO(os.stat(fifo).st_mode):
        with open(fifo, 'w') as pipeout:
            pipeout.write("%s\n" % filename)
    else:
        targetdir = dir + '/' + now.strftime("%Y-%m-%d")
        target    = targetdir + '/' + filename
        # ensure that the image directory exists
        if not Path(targetdir).exists():
            Path(targetdir).mkdir(parents=True)
        # mv to target directory
        os.rename(fullname, target)

    configfile = open(inifile_name, 'w')
    config.write(configfile)
    configfile.close()

    print("date=%s; time=%s; file=%s; ex=%d; iso=%d; br=%s; sat=%d; co=%d img brightness=%d" % (now.strftime("%Y-%m-%d"), now.strftime("%H:%M:%S"), filename, imgExpTime, iso, brightness, saturation, contrast, imgBrightness))

if __name__ == "__main__":
    try:
        main()
    except PidFileError:
        print ("Capture still running")
