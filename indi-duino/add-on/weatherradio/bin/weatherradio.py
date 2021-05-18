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
import rrdtool
from wr_config import *

# initialize the configuration
wrConfig = WeatherRadioConfig()
config   = wrConfig.config

def connect(indi, deviceID, verbose=False):
    # read device properties from config file
    dev       = config.get(deviceID, 'INDIDEVICE')
    mode      = config.get(deviceID, 'INDIDEVICEMODE')
    devport   = config.get(deviceID, 'INDIDEVICEPORT', fallback=None)
    ipaddress = config.get(deviceID, 'INDI_IP_ADDRESS', fallback=None)
    ipport    = config.get(deviceID, 'INDI_IP_PORT', fallback=None)
    lat       = config.get(deviceID, 'GEO_COORD_LAT')
    long      = config.get(deviceID, 'GEO_COORD_LONG')
    elev      = config.get(deviceID, 'GEO_COORD_ELEV')

    # ensure that all devices are connected
    connection = indi.get_vector(dev, "CONNECTION")
    if connection.get_element("CONNECT").get_active() == False:
        # select the connection mode
        if mode == "Serial":
            # ensure serial mode
            indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION_MODE","Serial")
            # set the configured port
            indi.set_and_send_text(dev,"DEVICE_PORT","PORT",devport)
            if verbose:
                print ("Setting serial port to %s" % (devport))
        else:
            # set connection mode to Ethernet
            indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION_MODE","Ethernet")
            indi.set_and_send_text(dev,"DEVICE_ADDRESS","ADDRESS",ipaddress)
            indi.set_and_send_text(dev,"DEVICE_ADDRESS","PORT",ipport)
            if verbose:
                print ("Setting ethernet ip:port to %s:%s" % (ipaddress, ipport))

        # connect driver
        connection = indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION","Connect")
        if verbose:
            print ("Waiting for connection...")
        # wait for the connection
        time.sleep(7)
        # ensure that all information is up to date
        indi.process_events()
        # check if the connection has been established
        connection = indi.get_vector(dev, "CONNECTION")
        # set location if connection was successful
        if connection._light.is_ok():
            if verbose:
                print ("Connection succeeded.")
            indi.set_and_send_float(dev,"GEOGRAPHIC_COORD","LAT",float(lat))
            indi.set_and_send_float(dev,"GEOGRAPHIC_COORD","LONG",float(long))
            indi.set_and_send_float(dev,"GEOGRAPHIC_COORD","ELEV",float(elev))
            if verbose:
                print ("Setting location to lat=%s, long=%s, elev=%s." % (lat, long, elev))
                
    return connection._light.is_ok()

def read_indi_value(result, key, vector, element):
    if (vector.get_element(element) is not None):
        result[key] = vector.get_element(element).get_float()

def vector_exists(indi, device, name):
    for vector in indi.indivectors.list:
        if vector.name == name and vector.device == device:
            return True
    return False


def readWeather(indi, device, verbose=False):
    result  = {}
    # Push the "Refresh" button to ensure that all information is up to date
    indi.set_and_send_switchvector_by_elementlabel(device,"WEATHER_REFRESH","Refresh")
    indi.process_events()
    # read weather data from device
    weather = indi.get_vector(device, config.get('WeatherRadio', 'WEATHER'))
    # ensure that parameters are available
    if weather._light.is_ok():
        if verbose:
            print ("Reading weather parameter...")
        read_indi_value(result, 'Temperature', weather,
                        config.get('WeatherRadio', 'WEATHER_TEMPERATURE'))
        read_indi_value(result, 'Pressure', weather,
                        config.get('WeatherRadio', 'WEATHER_PRESSURE'))
        read_indi_value(result, 'Humidity', weather,
                        config.get('WeatherRadio', 'WEATHER_HUMIDITY'))
        read_indi_value(result, 'CloudCover', weather,
                        config.get('WeatherRadio', 'WEATHER_CLOUD_COVER'))
        read_indi_value(result, 'SQM', weather,
                        config.get('WeatherRadio', 'WEATHER_SQM'))
        read_indi_value(result, 'DewPoint', weather,
                        config.get('WeatherRadio', 'WEATHER_DEWPOINT'))
        read_indi_value(result, 'SkyTemperature', weather,
                        config.get('WeatherRadio', 'WEATHER_SKY_TEMPERATURE'))
        read_indi_value(result, 'WindSpeed', weather,
                        config.get('WeatherRadio', 'WEATHER_WIND_SPEED'))
        read_indi_value(result, 'WindGust', weather,
                        config.get('WeatherRadio', 'WEATHER_WIND_GUST'))
        read_indi_value(result, 'WindDirection', weather,
                        config.get('WeatherRadio', 'WEATHER_WIND_DIRECTION'))
        read_indi_value(result, 'RaindropFrequency', weather,
                        config.get('WeatherRadio', 'WEATHER_RAIN_DROPS'))
        read_indi_value(result, 'RainVolume', weather,
                        config.get('WeatherRadio', 'WEATHER_RAIN_VOLUME'))
        if verbose:
            print ("Reading weather parameter... (succeeded)")
    else:
        print ("Reading weather parameter FAILED")

    return result


def readSensors(indi, device):
    result = {}
    indi.process_events()
    # read sensor data from device
    if vector_exists(indi, device, "BME280"):
        bme280 = indi.get_vector(device, "BME280")
        read_indi_value(result, 'BME280_Temp', bme280, 'Temp')
        read_indi_value(result, 'BME280_Pres', bme280, 'Pres')
        read_indi_value(result, 'BME280_Hum', bme280, 'Hum')

    if vector_exists(indi, device, "DHT"):
        dht = indi.get_vector(device, "DHT")
        read_indi_value(result, 'DHT_Temp', dht, 'Temp')
        read_indi_value(result, 'DHT_Hum', dht, 'Hum')

    if vector_exists(indi, device, "MLX90614"):
        mlx90614 = indi.get_vector(device, "MLX90614")
        read_indi_value(result, 'MLX90614_Tamb', mlx90614, 'T amb')
        read_indi_value(result, 'MLX90614_Tobj', mlx90614, 'T obj')

    if vector_exists(indi, device, "TSL2591"):
        tsl2591 = indi.get_vector(device, "TSL2591")
        read_indi_value(result, 'TSL2591_Lux', tsl2591, 'Lux')
        
    return result


def updateRRD(rrdfile, data):
    updateString="N"
    templateString=""

    if data is not None:
        for key in data.keys():
            updateString=updateString+":"+str(data[key])
            if templateString:
                templateString += ":"
            templateString += key

        ret = rrdtool.update(rrdfile, "--template", templateString ,updateString)
        if ret:
            print (rrdtool.error())
