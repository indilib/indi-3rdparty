#!/usr/bin/python
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013


import sys, os
import math
from indiclient import *
import time
import signal
import rrdtool
from meteoconfig import *
import simplejson
import gc
#from guppy import hpy

signal.signal(signal.SIGINT, signal.SIG_DFL)


def recv_indi_old():
	tim=time.localtime()
        HR=indi.get_float(INDIDEVICE,"HR","HR")
        Thr=indi.get_float(INDIDEVICE,"HR","T")
        P=indi.get_float(INDIDEVICE,"Presure","P")
        Tp=indi.get_float(INDIDEVICE,"Presure","T")
        IR=indi.get_float(INDIDEVICE,"IR","IR")
        Tir=indi.get_float(INDIDEVICE,"IR","T")
        dew=indi.get_float(INDIDEVICE,"Meteo","DEW")
        light=indi.get_float(INDIDEVICE,"SQM","SQM")
        T=indi.get_float(INDIDEVICE,"Meteo","T")
        clouds=indi.get_float(INDIDEVICE,"Meteo","clouds") 
        skyT=indi.get_float(INDIDEVICE,"Meteo","SkyT") 
        statusVector=indi.get_vector(INDIDEVICE,"STATUS")
	cloudFlag=int(statusVector.get_element("clouds").is_ok())
	dewFlag=int(statusVector.get_element("dew").is_ok())
	frezzingFlag=int(statusVector.get_element("frezzing").is_ok())
	return (("HR",HR),("Thr",Thr),("IR",IR),("Tir",Tir),("P",P),("Tp",Tp),("Dew",dew),("Light",light),
           ("T",T),("clouds",clouds),("skyT",skyT),("cloudFlag",cloudFlag),("dewFlag",dewFlag),
           ("frezzingFlag",frezzingFlag))

def recv_indi():
	tim=time.localtime()
        vectorHR=indi.get_vector(INDIDEVICE,"Humidity")
	HR=vectorHR.get_element("HR").get_float()
	Thr=vectorHR.get_element("T").get_float()

        vectorPressure=indi.get_vector(INDIDEVICE,"Pressure")
	P=vectorPressure.get_element("P").get_float()
	Tp=vectorPressure.get_element("T").get_float()

        vectorIR=indi.get_vector(INDIDEVICE,"IR")
	IR=vectorIR.get_element("IR").get_float()
	Tir=vectorIR.get_element("T").get_float()

        vectorMeteo=indi.get_vector(INDIDEVICE,"WEATHER_PARAMETERS")
	dew=vectorMeteo.get_element("WEATHER_DEWPOINT").get_float()
	clouds=vectorMeteo.get_element("WEATHER_CLOUD_COVER").get_float()
	T=vectorMeteo.get_element("WEATHER_TEMPERATURE").get_float()
        skyT=vectorMeteo.get_element("WEATHER_SKY_TEMPERATURE").get_float()

        vectorSQM=indi.get_vector(INDIDEVICE,"Sky Quality")
	sqm=vectorSQM.get_element("SQM").get_float()
   
        statusVector=indi.get_vector(INDIDEVICE,"WEATHER_STATUS")
	cloudFlag=int(statusVector.get_element("clouds").is_ok())
	dewFlag=int(statusVector.get_element("dew").is_ok())
	frezzingFlag=int(statusVector.get_element("frezzing").is_ok())
  
	return (("HR",HR),("Thr",Thr),("IR",IR),("Tir",Tir),("P",P),("Tp",Tp),("Dew",dew),("SQM",sqm),
           ("T",T),("clouds",clouds),("skyT",skyT),("cloudFlag",cloudFlag),("dewFlag",dewFlag),
           ("frezzingFlag",frezzingFlag))



############# MAIN #############
INDIPORT=int(INDIPORT)

indi=indiclient(INDISERVER,INDIPORT)
#indi.tell()
indi.process_events()
now=time.localtime()
json_dict={"TIME":time.strftime("%c",now)}
data=recv_indi()	
updateString="N"
for d in data:
	#print d[0],d[1]
	updateString=updateString+":"+str(d[1])
	json_dict[d[0]]=int(d[1]*100)/100.
#print updateString
ret = rrdtool.update(RRDFILE,updateString);
if ret:
	print rrdtool.error() 
x = simplejson.dumps(json_dict)
#print x
fi=open(CHARTPATH+"RTdata.json","w")
fi.write(x)
fi.close()
indi.quit()
del indi
del data
del json_dict 
collected = gc.collect()
#print "Garbage collector: collected %d objects." % (collected)
#h = hpy()
#print h.heap()



