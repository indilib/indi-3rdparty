#!/usr/bin/python
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013

from meteostation import *

############# MAIN #############

# ensure that the driver is connected

indi=indiclient(INDISERVER,int(INDIPORT))
connect(indi)
init()

now=time.localtime()
json_dict={"TIME":time.strftime("%c",now)}
data=recv_indi(indi)	
indi.quit()

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
del indi
del data
del json_dict 
collected = gc.collect()
#print "Garbage collector: collected %d objects." % (collected)
#h = hpy()
#print h.heap()



