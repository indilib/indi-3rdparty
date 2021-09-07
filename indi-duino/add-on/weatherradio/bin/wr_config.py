#!/usr/bin/python3
# -*- coding: latin-1 -*-

#-----------------------------------------------------------------------
# Configuration of Weather Radio.
#
# Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
#-----------------------------------------------------------------------

from configparser import ConfigParser
from os import path

class WeatherRadioConfig:
    config = None

    def __init__(self):
        # setup configuration
        self.config = ConfigParser(interpolation=None)
        self.config.optionxform = str
        # default values
        self.config.add_section('WeatherRadio')
        # web server configuration
        self.config.set('WeatherRadio', 'INDISERVER', '0.0.0.0')
        self.config.set('WeatherRadio', 'INDIPORT', '8624')

        # RRD storage holding time series data
        self.config.set('WeatherRadio', 'RRDFILE',
                        '/usr/local/share/weatherradio/weather.rrd')

        # RRD storage holding time series for raw sensor data
        self.config.set('WeatherRadio', 'RRDSENSORSFILE',
                        '/usr/local/share/weatherradio/sensors.rrd')
        # path to the JSON files holding the data
        self.config.set('WeatherRadio', 'DATAPATH',
                        '/usr/local/share/weatherradio/html/data')

        # path to the directory holding web cam images and videos
        self.config.set('WeatherRadio', 'MEDIADIR',
                        '/usr/local/share/weatherradio/html/media')

        ######### INDI Weather vector and element names
        self.config.set('WeatherRadio', 'WEATHER', 'WEATHER_PARAMETERS')
        self.config.set('WeatherRadio', 'WEATHER_TEMPERATURE', 'WEATHER_TEMPERATURE')
        self.config.set('WeatherRadio', 'WEATHER_PRESSURE', 'WEATHER_PRESSURE')
        self.config.set('WeatherRadio', 'WEATHER_HUMIDITY', 'WEATHER_HUMIDITY')
        self.config.set('WeatherRadio', 'WEATHER_CLOUD_COVER', 'WEATHER_CLOUD_COVER')
        self.config.set('WeatherRadio', 'WEATHER_SQM', 'WEATHER_SQM')
        self.config.set('WeatherRadio', 'WEATHER_DEWPOINT', 'WEATHER_DEWPOINT')
        self.config.set('WeatherRadio', 'WEATHER_SKY_TEMPERATURE', 'WEATHER_SKY_TEMPERATURE')
        self.config.set('WeatherRadio', 'WEATHER_WIND_GUST', 'WEATHER_WIND_GUST')
        self.config.set('WeatherRadio', 'WEATHER_WIND_SPEED', 'WEATHER_WIND_SPEED')
        self.config.set('WeatherRadio', 'WEATHER_WIND_DIRECTION', 'WEATHER_WIND_DIRECTION')
        self.config.set('WeatherRadio', 'WEATHER_RAIN_DROPS', 'WEATHER_RAIN_DROPS')
        self.config.set('WeatherRadio', 'WEATHER_RAIN_VOLUME', 'WEATHER_RAIN_VOLUME')
        self.config.set('WeatherRadio', 'WEATHER_WETNESS', 'WEATHER_WETNESS')

        # read config files
        for file in ['/etc/weatherradio.ini',
                     '/usr/local/share/weatherradio/weatherradio.ini']:
            self.config.read(file)

    def getDevices(self):
        """Delivers the section names of devices"""
        return [x for x in self.config.sections() if x.startswith('Device')]
