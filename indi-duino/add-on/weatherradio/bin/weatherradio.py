#-----------------------------------------------------------------------
# Python library for weather radio.
#
# Copyright (C) 2019-20 Wolfgang Reissenberger <sterne-jaeger@t-online.de>
#
# This application is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
#-----------------------------------------------------------------------

import time
from wr_config import *

def connect(indi):
    #connect ones to configure the port
    connection = indi.get_vector(INDIDEVICE, "CONNECTION")
    if connection.get_element("CONNECT").get_active() == False:
        # set the configured port
        indi.set_and_send_text(INDIDEVICE,"DEVICE_PORT","PORT",INDIDEVICEPORT)

        # connect driver
        connection = indi.set_and_send_switchvector_by_elementlabel(INDIDEVICE,"CONNECTION","Connect")
        # wait for the connection
        time.sleep(5)
        # ensure that all information is up to date
        indi.process_events()
        # check if the connection has been established
        connection = indi.get_vector(INDIDEVICE, "CONNECTION")

    return connection._light.is_ok()

def read_indi_value(result, key, vector, element):
    if (vector.get_element(element) is not None):
        result[key] = vector.get_element(element).get_float()


def readWeather(indi):
    result  = {}
    # ensure that all information is up to date
    indi.process_events()
    # ensure that parameters are available
    weather = indi.get_vector(INDIDEVICE,WEATHER)
    if weather._light.is_ok():
        read_indi_value(result, 'Temperature', weather, WEATHER_TEMPERATURE)
        read_indi_value(result, 'Pressure', weather, WEATHER_PRESSURE)
        read_indi_value(result, 'Humidity', weather, WEATHER_HUMIDITY)
        read_indi_value(result, 'CloudCover', weather, WEATHER_CLOUD_COVER)
        read_indi_value(result, 'SQM', weather, WEATHER_SQM)
        read_indi_value(result, 'DewPoint', weather, WEATHER_DEWPOINT)
        read_indi_value(result, 'SkyTemperature', weather, WEATHER_SKY_TEMPERATURE)
        read_indi_value(result, 'WindSpeed', weather, WEATHER_WIND_SPEED)
        read_indi_value(result, 'WindGust', weather, WEATHER_WIND_GUST)
        read_indi_value(result, 'WindDirection', weather, WEATHER_WIND_DIRECTION)

        return result;
    else:
        return None;


def readSensors(indi):
    result = {}
    bme280 = indi.get_vector(INDIDEVICE, "BME280")
    read_indi_value(result, 'BME280_Temp', bme280, 'Temp')
    read_indi_value(result, 'BME280_Pres', bme280, 'Pres')
    read_indi_value(result, 'BME280_Hum', bme280, 'Hum')

    dht = indi.get_vector(INDIDEVICE, "DHT")
    read_indi_value(result, 'DHT_Temp', dht, 'Temp')
    read_indi_value(result, 'DHT_Hum', dht, 'Hum')

    mlx90614 = indi.get_vector(INDIDEVICE, "MLX90614")
    read_indi_value(result, 'MLX90614_Tamb', mlx90614, 'T amb')
    read_indi_value(result, 'MLX90614_Tobj', mlx90614, 'T obj')

    tsl2591 = indi.get_vector(INDIDEVICE, "TSL2591")
    read_indi_value(result, 'TSL2591_Lux', tsl2591, 'Lux')

    return result;
