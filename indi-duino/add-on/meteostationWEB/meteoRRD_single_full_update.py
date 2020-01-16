#!/usr/bin/python
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013

from meteostation import *

############# MAIN #############

# ensure that the driver is connected

try:
    print "Updating data from \"%s\"@%s:%s" % (INDIDEVICE,INDISERVER,INDIPORT)
    indi=indiclient(INDISERVER,int(INDIPORT))
    connect(indi)
    init()

    now=time.localtime()
    json_dict={"TIME":time.strftime("%c",now)}
    data=recv_indi(indi)	
    indi.quit()
except:
    print "Updating data from \"%s\"@%s:%s FAILED!" % (INDIDEVICE,INDISERVER,INDIPORT)
    sys.exit()


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
print "Updating data from \"%s\"@%s:%s (succeeded)" % (INDIDEVICE,INDISERVER,INDIPORT)

################ update graphs ##################

print "Updating graphs"
graphs(3)
minutes = gmtime().tm_hour * gmtime().tm_min
if (minutes % 4 == 0):
	graphs(24)
if (minutes % 21 == 0):
	graphs(168)
if (minutes % 147 == 0):
	graphs(1176)
print "Updating graphs (succeeded)"





