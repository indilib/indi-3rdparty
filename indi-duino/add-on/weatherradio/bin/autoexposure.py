#!/usr/bin/python

#-----------------------------------------------------------------------
# Tool library for automatic exposure control
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

import ConfigParser as ConfigParser
from PIL import Image
from PIL import ImageStat
from PIL.ExifTags import TAGS

def dump_exif(exif):
  for (k,v) in exif.iteritems():
    print("%s: %s" % (TAGS.get(k), v))

def get_fields (exif, fields) :
  result = {}
  for (k,v) in exif.iteritems():
    tag = TAGS.get(k)
    if tag in fields:
      result[tag] = v
  return result

def get_field (exif,field) :
  result = get_fields(exif, [field])
  return result[field]

def brightness(image):
   im = image.convert('L')
   stat = ImageStat.Stat(im)
   return stat.rms[0]

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
    return config

def config_camera(camera, config):
    camera.shutter_speed =  config.getint('Camera', 'ExposureTime')
    camera.exposure_mode = 'fixedfps'
    camera.iso           = config.getint('Camera', 'ISOSpeedRatings')
    camera.brightness    = config.getint('Camera', 'Brightness')
    camera.contrast      = config.getint('Camera', 'Contrast')
    camera.saturation    = config.getint('Camera', 'Saturation')

def calculateExpTime(config, exptime, iso, brightness, contrast, saturation, img_brightness):
    factor = img_brightness / 120

    # avoid overshooting    
    factor = 0.85 if (factor < 0.85) else factor
    factor = 1.15 if (factor > 1.15) else factor

    newExpTime = int(round(exptime / factor**2))

    # we start with the old values
    newISO        = iso
    newBrightness = brightness
    newContrast   = contrast
    newSaturation = saturation
    if (newExpTime < 3000000 and iso > 50):
        # adapt ISO - less than 3 sec (avoid ISO flipping)
        newISO     /= 2
        newExpTime *= 2
    elif (newExpTime > 5000000 and iso < config.getint('Night', 'MaxISO')):
        # adapt ISO - more than 1 sec
        newISO     *= 2
        newExpTime /= 2

    # brightness and contrast depend upon ISO value
    newBrightness = 50 + int(config.getint('Night', 'Brightness') * (newISO - 50) / 750)
    newContrast   =  0 + int(config.getint('Night', 'Contrast') * (newISO - 50) / 750)

    # limit to maximal exposure value
    newExpTime = min(newExpTime, config.getint('Night', 'MaxExposure'))
        
    # reduce saturation for long exposures
    if (newExpTime > 2000000):
      newSaturation = int(config.getint('Night', 'Saturation') * (newExpTime - 2000000)/(config.getint('Night', 'MaxExposure') - 2000000))
    else:
      newSaturation = 0
      
    return (newExpTime, newISO, newBrightness, newContrast, newSaturation)

def calibrateExpTime(filename, config):
    # get image values
    image = Image.open(filename)
    exif = image._getexif()

    realExpTime    = 1000000 * get_field(exif, 'ExposureTime')[0] / get_field(exif, 'ExposureTime')[1]
    realISO        = get_field(exif, 'ISOSpeedRatings')
    bright         = brightness(image)
    realBrightness = config.getint('Camera', 'Brightness')
    realContrast   = config.getint('Camera', 'Contrast')
    realSaturation = config.getint('Camera', 'Saturation')

    # calculate the optimal exposure time
    (newExpTime, newISO, newBrightness, newContrast, newSaturation) = calculateExpTime(config, realExpTime, realISO, realBrightness, realContrast, realSaturation, bright)
    # store for future use
    config.set('Camera', 'ExposureTime', str(newExpTime))
    config.set('Camera', 'ISOSpeedRatings', str(newISO))
    config.set('Camera', 'Brightness', str(newBrightness))
    config.set('Camera', 'Contrast', str(newContrast))
    config.set('Camera', 'Saturation', str(newSaturation))
    return (realExpTime, bright)

