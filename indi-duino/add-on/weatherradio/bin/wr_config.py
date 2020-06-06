#!/usr/bin/python
# -*- coding: latin-1 -*-

#-----------------------------------------------------------------------
# Configuration for weather radio.
#
# Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@t-online.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------


# INDI server delivering weather data
INDISERVER="localhost"
INDIPORT="7624"

# INDI device
INDIDEVICE="Weather Radio"
INDIDEVICEPORT="/dev/ttyUSB0"

# RRD storage holding time series data
RRDFILE="/usr/share/weatherradio/weather.rrd"

# RRD storage holding time series for raw sensor data
RRDSENSORSFILE="/usr/share/weatherradio/sensors.rrd"

# path to the JSON files holding the data
DATAPATH="/usr/share/weatherradio/html/data"

# path to the directory holding web cam images and videos
MEDIADIR="/usr/share/weatherradio/html/media"

######### INDI Weather vector and element names
WEATHER="WEATHER_PARAMETERS"
WEATHER_TEMPERATURE="WEATHER_TEMPERATURE"
WEATHER_PRESSURE="WEATHER_PRESSURE"
WEATHER_HUMIDITY="WEATHER_HUMIDITY"
WEATHER_CLOUD_COVER="WEATHER_CLOUD_COVER"
WEATHER_SQM="WEATHER_SQM"
WEATHER_DEWPOINT="WEATHER_DEWPOINT"
WEATHER_SKY_TEMPERATURE="WEATHER_SKY_TEMPERATURE"
WEATHER_WIND_GUST="WEATHER_WIND_GUST"
WEATHER_WIND_SPEED="WEATHER_WIND_SPEED"
WEATHER_WIND_DIRECTION="WEATHER_WIND_DIRECTION"
