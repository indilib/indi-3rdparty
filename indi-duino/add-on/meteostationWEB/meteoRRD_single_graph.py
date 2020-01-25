#!/usr/bin/python
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013

from meteostation import *

P0=math.floor(1013.25/math.exp(ALTITUDE/8000.))
Pdelta=25
Pmin=P0-Pdelta
Pmax=P0+Pdelta


# main call

graphs(3)
minutes = gmtime().tm_hour * gmtime().tm_min
if (minutes % 4 == 0):
	graphs(24)
if (minutes % 21 == 0):
	graphs(168)
if (minutes % 147 == 0):
	graphs(1176)

