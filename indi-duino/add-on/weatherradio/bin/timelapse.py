#!/usr/bin/python

import io
import os
import sys
import math
import time
from datetime import datetime
from pathlib import Path
import picamera
from PIL import Image
from fractions import Fraction
from autoexposure import *

def get_image_name(now):
    dir = config.get('Camera', 'BaseDirectory') + '/' + now.strftime("%Y-%m-%d")
    # ensure that the image directory exists
    if not Path(dir).exists():
        Path(dir).mkdir(parents=True)
    filename =  dir + '/' + '{timestamp:%Y-%m-%d_%H%M%S}.jpg'
    return filename

config = init_config()
# additional default values
config.set('Camera', 'resX', '3280')
config.set('Camera', 'resY', '2464')

# read config file
inifile_name = os.path.dirname(os.path.realpath(__file__)) + '/timelapse.ini'
config.read(inifile_name)

stopfile = Path('/tmp/timelapse.stop')

# shoot images
with picamera.PiCamera() as camera:
    # set base parameters that cannot be changed during continuous capturing
    camera.resolution    = (config.getint('Camera', 'resX'),
                            config.getint('Camera', 'resY'))
    camera.framerate = Fraction (1, int(math.ceil(config.getint('Night', 'MaxExposure') / 1000000)))

    #update the camera settings from the configuration
    config_camera(camera, config)

    # start measuring total capture time
    start = time.time()
    now   = datetime.fromtimestamp(start)
        
    # continuous capturing loop
    for filename in camera.capture_continuous(get_image_name(now)):

        # abort if stop file exists
        if stopfile.is_file():
            print("Good bye")
            break

        # remember value before calibration
        iso = config.getint('Camera', 'ISOSpeedRatings')

        # calculate the optimal exposure time etc.
        (exp_speed, brightness) = calibrateExpTime(filename, config)

        print ("date=%s; time=%s; file=%s; ex=%d; iso=%d; bright=%s" % (now.strftime("%Y-%m-%d"), now.strftime("%H:%M:%S"), filename, exp_speed, iso, brightness))

        # wait 30 seconds
        sleep = max(30 - time.time() + start, 2)
        # print "Sleeping %d seconds" % (sleep)
        time.sleep(sleep) 
        start = time.time()
        now   = datetime.fromtimestamp(start)

        # change the configuration
        config_camera(camera, config)

# update the config file
configfile = open(inifile_name, 'w')
config.write(configfile)
configfile.close()


