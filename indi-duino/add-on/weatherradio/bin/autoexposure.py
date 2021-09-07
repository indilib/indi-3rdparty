#!/usr/bin/python3

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

from PIL import Image
from PIL import ImageStat
from PIL.ExifTags import TAGS

def dump_exif(exif):
  for (k,v) in exif.items():
    print("%s: %s" % (TAGS.get(k), v))

def get_fields (exif, fields) :
  result = {}
  for (k,v) in exif.items():
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

def calculateExpTime(config, exptime, iso, brightness, contrast, saturation, img_brightness):
    factor = img_brightness / 120

    # avoid overshooting
    factor = 0.7 if (factor < 0.7) else factor
    factor = 1.3 if (factor > 1.3) else factor

    #newExpTime = exptime / factor**2
    # smoothen reaction
    newExpTime = exptime / factor

    # we start with the old values
    newISO        = iso
    newBrightness = brightness
    newContrast   = contrast
    newSaturation = saturation
    if (newExpTime < 100000 and iso > 50):
        # adapt ISO - less than 3 sec (avoid ISO flipping)
        newISO     /= 2
        newExpTime = newExpTime * 1.4
    elif (newExpTime > 500000 and iso < config.getint('Night', 'MaxISO')):
        # adapt ISO - more than 1 sec
        newISO     *= 2
        newExpTime = newExpTime / 1.4
    else:
        # adapt only if no ISO change happened to avoid miscorrections
        # target brightness depends upon ISO value, 50 for daytime as target
        newBrightness = 50 + int((config.getint('Night', 'Brightness') - 50) * (newISO - 50) / 750)
        # change brightness and contrast slowly
        # brightness + 10 equals exptime * 2
        if newBrightness > brightness:
            newBrightness = brightness + 1
        elif newBrightness < brightness:
            newBrightness = brightness - 1

    # target contrast also depends upon ISO value, but does not impact brightness
    newContrast = 0 + int(config.getint('Night', 'Contrast') * (newISO - 50) / 750)
    if newContrast > contrast:
        newContrast = min(contrast + 2, newContrast)
    elif newContrast < contrast:
        newContrast = max(contrast - 2, newContrast)

    # limit to maximal exposure value
    newExpTime = min(newExpTime, config.getint('Night', 'MaxExposure'))

    # reduce saturation for long exposures
    if (newExpTime > 2000000):
      newSaturation = int(config.getint('Night', 'Saturation') * (newExpTime - 2000000)/(config.getint('Night', 'MaxExposure') - 2000000))
    else:
      newSaturation = 0

    # change saturation slowly
    if newSaturation > saturation:
      newSaturation = saturation + 1
    elif newSaturation < saturation:
      newSaturation = saturation - 1

    return (newExpTime, newISO, newBrightness, newContrast, newSaturation)

def calibrateExpTime(filename, config):
    # get image values
    image = Image.open(filename)
    exif = image._getexif()

    exifexptime    =  get_field(exif, 'ExposureTime')
    #print("exif:ExposureTime = %d/%d" % exifexptime)

    realExpTime    = 1000000 * exifexptime[0] / exifexptime[1]
    expTime        = config.getint('Camera', 'ExposureTime')
    # long exposure times cause trouble
    if expTime > 1000000:
        realExpTime = expTime

    realISO        = config.getint('Camera', 'ISOSpeedRatings')
    bright         = brightness(image)
    realBrightness = config.getint('Camera', 'Brightness')
    realContrast   = config.getint('Camera', 'Contrast')
    realSaturation = config.getint('Camera', 'Saturation')

    # calculate the optimal exposure time
    (newExpTime, newISO, newBrightness, newContrast, newSaturation) = calculateExpTime(config, realExpTime, realISO, realBrightness, realContrast, realSaturation, bright)
    # store for future use
    config.set('Camera', 'ExposureTime', str(int(round(newExpTime))))
    config.set('Camera', 'ISOSpeedRatings', str(int(round(newISO))))
    config.set('Camera', 'Brightness', str(int(round(newBrightness))))
    config.set('Camera', 'Contrast', str(int(round(newContrast))))
    config.set('Camera', 'Saturation', str(int(round(newSaturation))))
    return (realExpTime, bright)

