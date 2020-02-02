#!/usr/bin/python

#-----------------------------------------------------------------------
# Script for updating the RRD time series.
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

import sys
from indiclient import *
from weatherradio import *
import rrdtool

try:
    print "Updating data from \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)

    # open connection to the INDI server
    indi=indiclient(INDISERVER,int(INDIPORT))

    # ensure that the INDI driver is connected to the device
    connect(indi)
    
    now=time.localtime()
    json_dict={"TIME":time.strftime("%c",now)}
    data = readWeather(indi)

    indi.quit()

except:
    print "Updating data from \"%s\"@%s:%s FAILED!" % (INDIDEVICE,INDISERVER,INDIPORT)
    sys.exit()


updateString="N"
for d in data:
	# print d[0],d[1]
	updateString=updateString+":"+str(d[1])
	json_dict[d[0]]=int(d[1]*100)/100.

ret = rrdtool.update(RRDFILE,updateString);
if ret:    
	print rrdtool.error() 
