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

import io, os, stat, sys, math
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
    oldname      = None
    rates        = []
    last_shot    = 0

    def __init__(self):
        # setup configuration
        self.config = ConfigParser(interpolation=None)
        self.config.optionxform = str
        # default values
        self.config.add_section('Camera')
        self.config.set('Camera', 'SensorMode', '3')
        self.config.set('Camera', 'ExposureTime', '400') # 1/250 sec
        self.config.set('Camera', 'resX', '3280')
        self.config.set('Camera', 'resY', '2464')
        self.config.set('Camera', 'BaseDirectory', ".")
        self.config.set('Camera', 'ConverterFIFO', "/tmp/imageconverter.fifo")
        self.config.set('Camera', 'ISOSpeedRatings', '50')
        self.config.set('Camera', 'Contrast', '0')
        self.config.set('Camera', 'Brightness', '50')
        self.config.set('Camera', 'Saturation', '0')
        self.config.set('Camera', 'interval', '60')
        self.config.set('Camera', 'zoomX0', '0.0')
        self.config.set('Camera', 'zoomY0', '0.0')
        self.config.set('Camera', 'zoomX1', '1.0')
        self.config.set('Camera', 'zoomY1', '1.0')
        self.config.set('Camera', 'ExposureTimeout', '120')

        # night default settings
        self.config.add_section('Night')
        self.config.set('Night', 'Contrast', '100')
        self.config.set('Night', 'Brightness', '78')
        self.config.set('Night', 'Saturation', '-80')
        self.config.set('Night', 'MaxExposure', '20000000') # 20 sec
        self.config.set('Night', 'MaxISO', '800')

        # read ini file
        self.inifile_name = os.path.dirname(os.path.realpath(__file__)) + '/timelapse.ini'
        self.config.read(self.inifile_name)

        self.stopfile = Path('/tmp/timelapse.stop')


    def config_camera(self, camera):
        # change to auto exposure for short exposure times
        if self.config.getint('Camera', 'ExposureTime') < 10000:
            camera.shutter_speed = 0 # auto
        else:
            camera.shutter_speed = self.config.getint('Camera', 'ExposureTime')

        camera.iso           = self.config.getint('Camera', 'ISOSpeedRatings')
        camera.contrast      = self.config.getint('Camera', 'Contrast')
        camera.brightness    = self.config.getint('Camera', 'Brightness')
        camera.saturation    = self.config.getint('Camera', 'Saturation')
        camera.zoom          = (self.config.getfloat('Camera', 'zoomX0'),
                                self.config.getfloat('Camera', 'zoomY0'),
                                self.config.getfloat('Camera', 'zoomX1'),
                                self.config.getfloat('Camera', 'zoomY1'))

    def get_capture_dir(self):
        dir = self.config.get('Camera', 'BaseDirectory')
        # ensure that the image directory exists
        if not Path(dir).exists():
            Path(dir).mkdir(parents=True)
        return dir

    def get_target_dir(self, now):
        targetdir = self.config.get('Camera', 'BaseDirectory') + '/' + now.strftime("%Y-%m-%d")
        # ensure that the image directory exists
        if not Path(targetdir).exists():
            Path(targetdir).mkdir(parents=True)
        return targetdir

    def get_image_name(self, now, dir=None):
        filename = now.strftime("%Y-%m-%d_%H%M%S") + ".jpg"
        if dir != None:
            filename = dir + '/' + filename
        return filename


    def convert_image(self, now, fullname):
        if not self.stopfile.is_file():
            fifo = self.config.get('Camera', 'ConverterFIFO')
            if os.path.exists(fifo) and stat.S_ISFIFO(os.stat(fifo).st_mode):
                with open(fifo, 'w') as pipeout:
                    pipeout.write("%s\n" % self.get_image_name(now))
            else:
                # otherwise move image to target directory
                target = self.get_image_name(now, dir=self.get_target_dir(now))
                # mv to target directory
                os.rename(fullname, target)
            # remember the current name
            self.oldname = fullname


    def wait_for_converter(self, fullname):
        # wait only if there exists a pipe to the converter
        fifo = self.config.get('Camera', 'ConverterFIFO')
        if os.path.exists(fifo) and stat.S_ISFIFO(os.stat(fifo).st_mode):
            # wait until old image has disappeared
            while self.oldname is not None and Path(self.oldname).is_file() and not self.stopfile.is_file():
                print("%s waiting for image converter ..." % fullname)
                sleep(1)

    def single_shot(self, interval, camera):
        # set base parameters that cannot be changed
        self.config_camera(camera)
        # calculate wait time in seconds
        ts_now = time()
        # ensure that diff > interval and or on a 5 secs edge
        diff = ts_now - self.last_shot
        if diff > interval:
            # next 5 secs interval
            diff = (int(ts_now / 5)+1)*5-ts_now
        else:
            # fill up until interval
            diff = interval - diff
        sleep(diff)
        # current capture time
        ts_now = time()
        # start capturing
        start_capture = perf_counter()
        #print("Start capturing...")
        now = datetime.fromtimestamp(ts_now)
        fullname = self.get_image_name(now, dir=self.get_capture_dir())
        camera.capture(fullname)
        self.last_shot = ts_now

        # wait for the conversion of the previous image to be completed
        self.wait_for_converter(fullname)
        # calculate the optimal exposure time
        (imgExpTime, imgBrightness) = calibrateExpTime(fullname, self.config)

        # store new configuration
        configfile = open(self.inifile_name, 'w')
        self.config.write(configfile)
        configfile.close()

        print("date=%s time=%s file=%s ex=%d iso=%d br=%s sat=%d co=%d img_brightness=%d sleep=%0.1f speed=%d" % (now.strftime("%Y-%m-%d"), now.strftime("%H:%M:%S"), fullname, imgExpTime, camera.iso, camera.brightness, camera.saturation, camera.contrast, imgBrightness, diff, camera.shutter_speed))

        # start converter process
        self.convert_image(now, fullname)


    def settle(self, duration):
        print("Settle capturing... waiting for %0.1fs" % (duration / 1000000))
        sleep (int(duration / 1000000))


    def start(self, interval):
        # initial configuration
        sensor_mode = self.config.getint('Camera', 'SensorMode')
        resolution = (self.config.getint('Camera', 'resX'),
                      self.config.getint('Camera', 'resY'))

        # manually set the timeout, necessary for long exposures
        PiCamera.CAPTURE_TIMEOUT = self.config.getint('Camera', 'ExposureTimeout')
        # derive the frame rate range from longest exposure
        fr = (Fraction(1, int(self.config.getint('Night', 'MaxExposure')/1000000)), Fraction(1, 1))

        # shoot images
        with PiCamera(framerate_range=fr, resolution=resolution,
                      sensor_mode=sensor_mode) as camera:
            while True:
                # abort if stop file exists
                if self.stopfile.is_file():
                    print ("Stopping timelapse")
                    break
                else:
                    # single shot
                    self.single_shot(interval, camera)

            # sleeping a while until everything is settled
            self.settle(1.5*camera.exposure_speed)

        if self.stopfile.is_file():
            # we are finished
            os.remove(self.stopfile)


if __name__ == "__main__":
    service = TimelapseService()
    service.start(service.config.getint('Camera', 'interval'))
