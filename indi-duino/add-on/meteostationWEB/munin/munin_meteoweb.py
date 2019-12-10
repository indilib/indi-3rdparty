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

def getRawParameter(indi):
        vectorIR=indi.get_vector(INDIDEVICE,"IR")
	IR=vectorIR.get_element("IR").get_float()
	Tir=vectorIR.get_element("T").get_float()

        vectorHR=indi.get_vector(INDIDEVICE,"Humidity")
	HR=vectorHR.get_element("HR").get_float()
	Thr=vectorHR.get_element("T").get_float()

        vectorPressure=indi.get_vector(INDIDEVICE,"Pressure")
	P=vectorPressure.get_element("P").get_float()
	Tp=vectorPressure.get_element("T").get_float()

	return {"HR":HR, "Thr":Thr, "IR":IR, "Tir":Tir, "P":P, "Tp":Tp}


def getWeatherStatus(indi):
        vectorMeteo=indi.get_vector(INDIDEVICE,"WEATHER_PARAMETERS")
	dew=vectorMeteo.get_element("WEATHER_DEWPOINT").get_float()
	clouds=vectorMeteo.get_element("WEATHER_CLOUD_COVER").get_float()
	T=vectorMeteo.get_element("WEATHER_TEMPERATURE").get_float()
        skyT=vectorMeteo.get_element("WEATHER_SKY_TEMPERATURE").get_float()

        statusVector=indi.get_vector(INDIDEVICE,"WEATHER_STATUS")
	cloudFlag=int(statusVector.get_element("clouds").is_ok())
	dewFlag=int(statusVector.get_element("dew").is_ok())
	frezzingFlag=int(statusVector.get_element("frezzing").is_ok())
  
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

