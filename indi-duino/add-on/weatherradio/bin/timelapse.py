#!/usr/bin/python3

#-----------------------------------------------------------------------
# Time lapse service for shooting continuously single images with
# automatic exposure adaption day and night.
#
# Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

import io, os, sys, math
from time import time, sleep, perf_counter
from datetime import datetime
from configparser import ConfigParser
from pathlib import Path
from picamera import PiCamera
from fractions import Fraction
from autoexposure import *

class TimelapseService:
    config       = None
    inifile_name = None
    stopfile     = None
    rates        = {}
    
    def __init__(self):
        # setup configuration
        self.config = ConfigParser(interpolation=None)
        self.config.optionxform = str
        # default values
        self.config.add_section('Camera')
        self.config.set('Camera', 'resX', '3280')
        self.config.set('Camera', 'resY', '2464')
        self.config.set('Camera', 'ExposureTime', '400') # 1/250 sec
        self.config.set('Camera', 'BaseDirectory', ".")
        self.config.set('Camera', 'ConverterFIFO', "/tmp/imageconverter.fifo")
        self.config.set('Camera', 'ISOSpeedRatings', '50')
        self.config.set('Camera', 'Contrast', '0')
        self.config.set('Camera', 'Brightness', '50')
        self.config.set('Camera', 'Saturation', '0')
        # night default settings
        self.config.add_section('Night')
        self.config.set('Night', 'Contrast', '100')
        self.config.set('Night', 'Brightness', '20')
        self.config.set('Night', 'Saturation', '-80')
        self.config.set('Night', 'MaxExposure', '10000000')
        self.config.set('Night', 'MaxISO', '800')

        # read ini file
        self.inifile_name = os.path.dirname(os.path.realpath(__file__)) + '/timelapse.ini'
        self.config.read(self.inifile_name)

        self.stopfile = Path('/tmp/timelapse.stop')

        # define frame rates for short and long exposures
        self.rates['short'] = Fraction(1, 2)
        self.rates['long']  = Fraction (1000000,
                                        self.config.getint('Night',
                                                           'MaxExposure'))


    def config_camera(self, camera):
        camera.shutter_speed = self.config.getint('Camera', 'ExposureTime')
        camera.iso           = self.config.getint('Camera', 'ISOSpeedRatings')
        camera.contrast      = self.config.getint('Camera', 'Contrast')
        camera.brightness    = self.config.getint('Camera', 'Brightness')
        camera.saturation    = self.config.getint('Camera', 'Saturation')
        
    
    def get_image_name(self, now):
        dir = self.config.get('Camera', 'BaseDirectory') + '/' + now.strftime("%Y-%m-%d")
        # ensure that the image directory exists
        if not Path(dir).exists():
            Path(dir).mkdir(parents=True)
        filename =  dir + '/' + now.strftime("tl_%Y-%m-%d_%H%M%S") + ".jpg"
        return filename

    def calculate_framerate(self):
        if self.config.getint('Camera', 'ExposureTime') <= 1000000:
            return self.rates['short']
        else:
            return self.rates['long']


    def single_shot(self, interval, camera):
        # set base parameters that cannot be changed
        self.config_camera(camera)
        # start measuring total capture time
        # calculate wait time in seconds
        start = time()
        diff = (int(start / interval)+1)*interval - start
        #print("Sleeping %0.1fs..." % diff)
        sleep(diff)
        # start capturing
        start_capture = perf_counter()
        #print("Start capturing...")
        now = datetime.fromtimestamp(start+diff)
        fullname = self.get_image_name(now)
        camera.capture(fullname)
        #print("Capture finished after %0.1fs, exp = %0.1f" % (perf_counter()-start_capture, camera.shutter_speed))
        # calculate the optimal exposure time
        (imgExpTime, imgBrightness) = calibrateExpTime(fullname, self.config)
        # store new configuration
        configfile = open(self.inifile_name, 'w')
        self.config.write(configfile)
        configfile.close()
        print("date=%s; time=%s; file=%s ex=%d iso=%d br=%s sat=%d co=%d img_brightness=%d sleep=%0.1f" % (now.strftime("%Y-%m-%d"), now.strftime("%H:%M:%S"), fullname, imgExpTime, camera.iso, camera.brightness, camera.saturation, camera.contrast, imgBrightness, diff))


    def settle(self):
        print("Settle capturing... waiting for %0.1fs" % (2*float(1/self.rates['long'])))
        sleep (2*float(1/self.rates['long']))



    def start(self, interval):
        # initial configuration
        framerate  = self.calculate_framerate()
        resolution = (self.config.getint('Camera', 'resX'),
                      self.config.getint('Camera', 'resY'))

        while True:
            # shoot images
            with PiCamera(framerate=framerate, resolution=resolution, sensor_mode=3) as camera:
                while True:
                    # abort if stop file exists
                    if self.stopfile.is_file() or framerate != self.calculate_framerate():
                        break
                    else:
                        # single shot
                        self.single_shot(interval, camera)
                # sleeping a while until everything is settled
                self.settle()

            if self.stopfile.is_file():
                # we are finished
                os.remove(self.stopfile)
                break
            else:
                # select new frame rate and continue
                framerate = self.calculate_framerate()


if __name__ == "__main__":
    service = TimelapseService()
    service.start(60)



