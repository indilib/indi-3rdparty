#-----------------------------------------------------------------------
# Python library for weather radio.
#
# Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------

from wr_config import *

def connect(indi):
    #connect ones to configure the port
    connection = indi.get_vector(INDIDEVICE, "CONNECTION")
    if connection.get_element("CONNECT").get_active() == False:
        # set the configured port
        indi.set_and_send_text(INDIDEVICE,"DEVICE_PORT","PORT",INDIDEVICEPORT)

        # connect driver
        connection.set_by_elementname("CONNECT")
        indi.send_vector(connection)
        print "CONNECT INDI Server host:%s port:%s device:%s" % (INDISERVER,INDIPORT,INDIDEVICE)
        time.sleep(5)

def readWeather(indi):
    weather     = indi.get_vector(INDIDEVICE,WEATHER)
    temperature = weather.get_element(WEATHER_TEMPERATURE).get_float()
    pressure    = weather.get_element(WEATHER_PRESSURE).get_float()
    humidity    = weather.get_element(WEATHER_HUMIDITY).get_float()
    cloudCover  = weather.get_element(WEATHER_CLOUD_COVER).get_float()
    sqm         = weather.get_element(WEATHER_SQM).get_float()
    dewpoint    = weather.get_element(WEATHER_DEWPOINT).get_float()
    skyTemp     = weather.get_element(WEATHER_SKY_TEMPERATURE).get_float()
  
    return (("Temperature", temperature), ("Pressure", pressure),
            ("Humidity", humidity), ("CloudCover", cloudCover),
            ("SkyTemperature", skyTemp), ("Dewpoint", dewpoint), ("SQM", sqm));

