#!/usr/bin/python3
# -*- coding: latin-1 -*-

#-----------------------------------------------------------------------
# Controller for the Adafruit Mini Pan-Tilt Kit, inspired by
# https://github.com/sparkfun/RPi_PanTilt_Camera_Kit.git
#
# Copyright (C) 2020, 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#-----------------------------------------------------------------------

import RPi.GPIO as GPIO
import configparser as ConfigParser
import time
import os

PIN_PAN    = 11
PIN_TILT   = 12
stepsize   = 2.0
currentpos = {}
range      = {}
servo_pan  = None
servo_tilt = None
configfile = os.path.dirname(os.path.realpath(__file__)) + '/pan_tilt_controller.ini'
# initialize the configuration
def init_config():
    global currentpos, range

    config = ConfigParser.ConfigParser()
    config.optionxform = str
    # default values
    config.add_section('PanTilt')
    config.set('PanTilt', 'PosX', '90')
    config.set('PanTilt', 'minX', '10')
    config.set('PanTilt', 'maxX', '170')
    config.set('PanTilt', 'minY', '10')
    config.set('PanTilt', 'maxY', '170')
    config.set('PanTilt', 'CmdFIFO', '/tmp/FIFO_pantilt')
    # read config file or create it if missing
    if os.path.isfile(configfile):
        config.read(configfile)

    # update the config file
    configfp = open(configfile, 'w')
    config.write(configfp)
    configfp.close()

    currentpos['x'] = config.getint('PanTilt', 'PosX')
    currentpos['y'] = config.getint('PanTilt', 'PosY')

    range['minX'] = config.getint('PanTilt', 'minX')
    range['maxX'] = config.getint('PanTilt', 'maxX')
    range['minY'] = config.getint('PanTilt', 'minY')
    range['maxY'] = config.getint('PanTilt', 'maxY')

    return config

# initialize the servos
def init_servos():
    global servo_pan, servo_tilt, currentpos

    # Set GPIO numbering mode
    GPIO.setmode(GPIO.BOARD)

    # Set pin as an output, and set servo1 as as PWM
    GPIO.setup(PIN_PAN,GPIO.OUT)
    GPIO.setup(PIN_TILT,GPIO.OUT)
    servo_pan  = GPIO.PWM(PIN_PAN,50) # 50 = 50Hz pulse
    servo_tilt = GPIO.PWM(PIN_TILT,50) # 50 = 50Hz pulse
    servo_pan.start(0)
    servo_tilt.start(0)


#Clean things up at the end
def cleanup():
    servo_pan.stop()
    servo_tilt.stop()
    GPIO.cleanup()
    print ("Goodbye")

# absolute move a servo to a position (in degrees)
def goto(servo, deg, sleep):
    #  3:   0 deg
    # 12: 180 deg
    servo.ChangeDutyCycle(10.0*deg/180 + 2.0)
    time.sleep(sleep)
    servo.ChangeDutyCycle(0)
    time.sleep(sleep)

def slew_slope(servo, current, target):
    dir = 1 if target > current else -1
    while dir * (target - current) > 0:
        # ensure that 0 <= pos <= 180
        pos = min(max(current + dir * stepsize, 0), 360)
        # ensure that we do not overshoot
        pos =  min(target, pos) if dir > 0 else max(target, pos)
        # slew one step
        goto(servo, pos, 0.02)
        # update the current position
        current = pos


# slew stepwise (in 2 deg steps) to a given position
def slew(x, y):
    global currentpos, range

    # first slew horizontally to avoid overloading the Raspberry Pi
    if (x >= range['minX']) and (x <= range['maxX']):
        slew_slope(servo_pan, currentpos['x'], x)
        currentpos['x'] = x
        config.set('PanTilt', 'PosX', str(currentpos['x']))
    else:
        print ("x=%d out of range [%d, %d]" % (x, range['minX'], range['maxX']))

    # now slew vertically to the target position
    if (y >= range['minY']) and (y <= range['maxY']):
        slew_slope(servo_tilt, currentpos['y'], y)
        currentpos['y'] = y
        config.set('PanTilt', 'PosY', str(currentpos['y']))
    else:
        print ("y=%d out of range [%d, %d]" % (y, range['minY'], range['maxY']))

    # update the config file
    configfp = open(configfile, 'w')
    config.write(configfp)
    configfp.close()


config = init_config()
init_servos()

# move to last position
goto(servo_pan,  config.getint('PanTilt', 'PosX'), 1.0)
goto(servo_tilt, config.getint('PanTilt', 'PosY'), 1.0)

while True:
    pipein = open(config.get('PanTilt', 'CmdFIFO'), 'r')
    line = pipein.readline()
    line_array = line.split(' ')
    if line_array[0] == "goto":
        slew(int(line_array[1]), int(line_array[2]))
    elif line.strip('\n') == "close":
        break
    elif line.strip('\n') == "stop":
        break
    else:
        print("unknown command: \"%s\"" % (line.strip('\n')))

pipein.close()
cleanup()
