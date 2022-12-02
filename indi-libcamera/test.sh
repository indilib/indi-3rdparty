rm -Rf /home/pi/.indi/logs/* 
indi_setprop LibCamera.LOG_OUTPUT.FILE_DEBUG=On && indi_setprop LibCamera.CONNECTION.CONNECT=On && indi_setprop LibCamera.CCD_EXPOSURE.CCD_EXPOSURE_VALUE=1 
cat /home/pi/.indi/logs/*/*.log