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
    result  = {}
    weather = indi.get_vector(INDIDEVICE,WEATHER)
    result['Temperature']    = weather.get_element(WEATHER_TEMPERATURE).get_float()
    result['Pressure']       = weather.get_element(WEATHER_PRESSURE).get_float()
    result['Humidity']       = weather.get_element(WEATHER_HUMIDITY).get_float()
    result['CloudCover']     = weather.get_element(WEATHER_CLOUD_COVER).get_float()
    result['SQM']            = weather.get_element(WEATHER_SQM).get_float()
    result['DewPoint']       = weather.get_element(WEATHER_DEWPOINT).get_float()
    result['SkyTemperature'] = weather.get_element(WEATHER_SKY_TEMPERATURE).get_float()
    result['WindSpeed']      = weather.get_element(WEATHER_WIND_SPEED).get_float()
    result['WindGust']      = weather.get_element(WEATHER_WIND_GUST).get_float()
    result['WindDirection']  = weather.get_element(WEATHER_WIND_DIRECTION).get_float()
  
    return result;

def readSensors(indi):
    result = {}
    bme280 = indi.get_vector(INDIDEVICE, "BME280")
    result['BME280_Temp'] = bme280.get_element('Temp').get_float();
    result['BME280_Pres'] = bme280.get_element('Pres').get_float();
    result['BME280_Hum'] = bme280.get_element('Hum').get_float();

    dht = indi.get_vector(INDIDEVICE, "DHT")
    result['DHT_Temp'] = dht.get_element('Temp').get_float();
    result['DHT_Hum'] = dht.get_element('Hum').get_float();

    mlx90614 = indi.get_vector(INDIDEVICE, "MLX90614")
    result['MLX90614_Tamb'] = mlx90614.get_element('T amb').get_float();
    result['MLX90614_Tobj'] = mlx90614.get_element('T obj').get_float();

    tsl2591 = indi.get_vector(INDIDEVICE, "TSL2591")
    result['TSL2591_Lux'] = tsl2591.get_element('Lux').get_float();

    return result;
