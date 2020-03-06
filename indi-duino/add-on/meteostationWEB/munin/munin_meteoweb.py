#!/usr/bin/python
# coding: utf-8

import sys, os
import math
from indiclient import *


# ===========================================================================
# Munin Plugin for a meteoweb INDI server
# ===========================================================================

# ======================== INDI server configuration ========================
INDISERVER="localhost"
INDIPORT="7624"
INDIDEVICE="Arduino MeteoStation"
# ======================== INDI server configuration ========================

# ======================== INDI Weather vector and element names ============
SENSOR_HUMIDITY="Humidity"
SENSOR_HUMIDITY_HUM="HR"
SENSOR_HUMIDITY_TEMP="T"
SENSOR_PRESSURE="Pressure"
SENSOR_PRESSURE_PRES="P"
SENSOR_PRESSURE_TEMP="T"
SENSOR_IR="IR"
SENSOR_IR_IR="IR"
SENSOR_IR_TEMP="T"
WEATHER="WEATHER_PARAMETERS"
WEATHER_DEWPOINT="WEATHER_DEWPOINT"
WEATHER_CLOUDS="WEATHER_CLOUD_COVER"
WEATHER_TEMP="WEATHER_TEMPERATURE"
WEATHER_SKY_TEMP="WEATHER_SKY_TEMPERATURE"
WEATHER_SQM="Sky Quality"
WEATHER_SQM_SQM="SQM"
WEATHER_STATUS="WEATHER_STATUS"
WEATHER_STATUS_CLOUDS="clouds"
WEATHER_STATUS_DEW="dew"
WEATHER_STATUS_TEMP="frezzing"

######### INDI Weather vector and element names
# SENSOR_HUMIDITY="BME280"
# SENSOR_HUMIDITY_HUM="Hum"
# SENSOR_HUMIDITY_TEMP="Temp"
# SENSOR_PRESSURE="BME280"
# SENSOR_PRESSURE_PRES="Pres"
# SENSOR_PRESSURE_TEMP="Temp"
# SENSOR_IR="MLX90614"
# SENSOR_IR_IR="T obj"
# SENSOR_IR_TEMP="T amb"
# WEATHER="WEATHER_PARAMETERS"
# WEATHER_DEWPOINT="WEATHER_DEWPOINT"
# WEATHER_CLOUDS="WEATHER_CLOUD_COVER"
# WEATHER_TEMP="WEATHER_TEMPERATURE"
# WEATHER_SKY_TEMP="WEATHER_SKY_TEMPERATURE"
# WEATHER_SQM="WEATHER_PARAMETERS"
# WEATHER_SQM_SQM="WEATHER_SQM"
# WEATHER_STATUS="WEATHER_STATUS"
# WEATHER_STATUS_CLOUDS="WEATHER_CLOUD_COVER"
# WEATHER_STATUS_DEW="WEATHER_HUMIDITY"
# WEATHER_STATUS_TEMP="WEATHER_TEMPERATURE"
# ======================== INDI Weather vector and element names ============

def getRawParameter(indi):
        vectorHR=indi.get_vector(INDIDEVICE,SENSOR_HUMIDITY)
	HR=vectorHR.get_element(SENSOR_HUMIDITY_HUM).get_float()
        Thr=vectorHR.get_element(SENSOR_HUMIDITY_TEMP).get_float()

        vectorPressure=indi.get_vector(INDIDEVICE, SENSOR_PRESSURE)
	P=vectorPressure.get_element(SENSOR_PRESSURE_PRES).get_float()
        Tp=vectorPressure.get_element(SENSOR_PRESSURE_TEMP).get_float()

        vectorIR=indi.get_vector(INDIDEVICE, SENSOR_IR)
	IR=vectorIR.get_element(SENSOR_IR_IR).get_float()
	Tir=vectorIR.get_element(SENSOR_IR_TEMP).get_float()

        vectorSQM=indi.get_vector(INDIDEVICE, WEATHER_SQM)
	sqm=vectorSQM.get_element(WEATHER_SQM_SQM).get_float()
   
	return {"HR":HR, "Thr":Thr, "IR":IR, "Tir":Tir, "P":P, "Tp":Tp}


def getWeatherStatus(indi):
        vectorMeteo=indi.get_vector(INDIDEVICE, WEATHER)
	dew=vectorMeteo.get_element(WEATHER_DEWPOINT).get_float()
	clouds=vectorMeteo.get_element(WEATHER_CLOUDS).get_float()
	T=vectorMeteo.get_element(WEATHER_TEMP).get_float()
        skyT=vectorMeteo.get_element(WEATHER_SKY_TEMP).get_float()

        statusVector=indi.get_vector(INDIDEVICE, WEATHER_STATUS)
        cloudFlag=int(not (statusVector.get_element(WEATHER_STATUS_CLOUDS).is_ok() or
                           statusVector.get_element(WEATHER_STATUS_CLOUDS).is_idle()))
        dewFlag=int(not (statusVector.get_element(WEATHER_STATUS_DEW).is_ok() or
                         statusVector.get_element(WEATHER_STATUS_DEW).is_idle()))
        frezzingFlag=int(not (statusVector.get_element(WEATHER_STATUS_TEMP).is_ok() or
                              statusVector.get_element(WEATHER_STATUS_TEMP).is_idle()))
  
	return {"T":T, "clouds":clouds, "skyT":skyT, "cloudFlag":cloudFlag, "dewFlag":dewFlag, "frezzingFlag":frezzingFlag}

weatherStatus={}
sensorData={}

def readData():
    #connect to INDI server
    indi=indiclient(INDISERVER,int(INDIPORT))
    vector=indi.get_vector(INDIDEVICE,"CONNECTION")
    vector.set_by_elementname("CONNECT")
    indi.send_vector(vector)
    
    sensorData.update(getRawParameter(indi))
    weatherStatus.update(getWeatherStatus(indi))

    indi.quit()

