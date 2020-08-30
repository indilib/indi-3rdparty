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

def connect(indi):
    # separate device configurations
    devices     = INDIDEVICE.split(",")
    modes       = INDIDEVICEMODE.split(",")
    deviceports = INDIDEVICEPORT.split(",")
    ipaddresses = INDI_IP_ADDRESS.split(",")
    ipports     = INDI_IP_PORT.split(",")
    result      = True

    # ensure that all devices are connected
    for dev, mode, devport, ipaddress, ipport in zip(devices, modes, deviceports, ipaddresses, ipports):
        connection = indi.get_vector(dev, "CONNECTION")
        if connection.get_element("CONNECT").get_active() == False:
            # select the connection mode
            if mode == "Serial":
                # ensure serial mode
                indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION_MODE","Serial")
                # set the configured port
                indi.set_and_send_text(dev,"DEVICE_PORT","PORT",devport)
            else:
                indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION_MODE","Ethernet")
                indi.set_and_send_text(dev,"DEVICE_ADDRESS","ADDRESS",ipaddress)
                indi.set_and_send_text(dev,"DEVICE_ADDRESS","PORT",ipport)

                # connect driver
                connection = indi.set_and_send_switchvector_by_elementlabel(dev,"CONNECTION","Connect")
                # wait for the connection
                time.sleep(7)
                # ensure that all information is up to date
                indi.process_events()
                # check if the connection has been established
                connection = indi.get_vector(dev, "CONNECTION")

        # update the result states
        result = result and connection._light.is_ok()

    return result

def read_indi_value(result, key, vector, element):
    if (vector.get_element(element) is not None):
        result[key] = vector.get_element(element).get_float()

def vector_exists(indi, device, name):
    for vector in indi.indivectors.list:
        if vector.name == name and vector.device == device:
            return True;
    return False;


def readWeather(indi):
    result  = {}
    # ensure that all information is up to date
    indi.process_events()
    # iterate over all configured devices
    for device in INDIDEVICE.split(","):
        weather = indi.get_vector(device,WEATHER)
        # ensure that parameters are available
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


def readSensors(indi):
    result = {}
    indi.process_events()
    # iterate over all configured devices
    for device in INDIDEVICE.split(","):
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
        
    return result;


def updateRRD(rrdfile, data):
    updateString="N"
    templateString=""

    if data is not None:
        for key in data.keys():
            updateString=updateString+":"+str(data[key])
            if templateString:
                templateString += ":"
            templateString += key

        ret = rrdtool.update(rrdfile, "--template", templateString ,updateString);
        if ret:
            print rrdtool.error()
