#!/usr/bin/env python3

import sys, time, logging
import PyIndi

RPI_CAMERA = 'RPI Camera'

typenames = {
    PyIndi.INDI_NUMBER: "NUMBER", 
    PyIndi.INDI_SWITCH: "SWITCH", 
    PyIndi.INDI_TEXT: "TEXT", 
    PyIndi.INDI_LIGHT: "LIGHT", 
    PyIndi.INDI_BLOB: "BLOB", 
    PyIndi.INDI_UNKNOWN: "UNKNOWN",
}

class IndiClient(PyIndi.BaseClient):
    def __init__(self):
        super(IndiClient, self).__init__()
        self.exposure = None
        self.gain = None
        self.done = False
        self.connection = None
        self.logger = logging.getLogger('PyQtIndi.IndiClient')
        self.logger.info('creating an instance of PyQtIndi.IndiClient')

    def newDevice(self, d):
        self.logger.info ("new Device '%s'" % d.getDeviceName())

    def newProperty(self, p):
        self.logger.info("%s: newProperty(%s)=%s(%s)" % (p.getDeviceName(), p.getName(), p.getNumber(), typenames[p.getType()]))

    def removeProperty(self, p):
        self.logger.info("%s: removeProperty(%s)=%s(%s)" % (p.getDeviceName(), p.getName(), p.getNumber(), typenames[p.getType()]))

    def newBLOB(self, bp):
        self.logger.info("new BLOB "+ bp.name)
        # get image data
        img = bp.getblobdata()
        # open a file and save buffer to disk
        with open("frame.fit", "wb") as f:
            f.write(img)
        self.done = True

    def newSwitch(self, svp):
        self.logger.info ("new Switch {} for device {}: {}".format(svp.name, svp.device, svp.s))

    def newNumber(self, nvp):
        self.logger.info("new Number {} for device {}: {}".format(nvp.name, nvp.device, nvp.np.value))

    def newText(self, tvp):
        self.logger.info("new Text "+ tvp.name + " for device "+ tvp.device)

    def newLight(self, lvp):
        self.logger.info("new Light "+ lvp.name + " for device "+ lvp.device)

    def newMessage(self, d, m):
        self.logger.info("newMessage(%s)=%s" % (d.getDeviceName(), d.messageQueue(m)))

    def serverConnected(self):
        print("Server connected ("+self.getHost()+":"+str(self.getPort())+")")
        self.connected = True

    def serverDisconnected(self, code):
        self.logger.info("Server disconnected (exit code = "+str(code)+","+str(self.getHost())+":"+str(self.getPort())+")")
        # set connected to False
        self.connected = False

    def takeExposure(self, seconds):
        device = self.getDevice(RPI_CAMERA)
        assert(device)
        exposure = device.getNumber("CCD_EXPOSURE")
        assert(exposure)
        exposure[0].value = seconds
        self.sendNewNumber(exposure)

logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

def setSwitch(client, name, n, value):
    switch = None
    device = client.getDevice(RPI_CAMERA)
    while True:
        switch = device.getSwitch(name)
        if switch:
            break
        print("Still waiting for {} switch".format(name))
        time.sleep(1)

    switch[n].s = value
    print("Sending switchvector {}".format(switch))
    print("Sending switch[0] {}".format(switch[0]))
    client.sendNewSwitch(switch)


if __name__ == '__main__':
    # Connect to indi-server
    indiclient = IndiClient()
    indiclient.setServer("192.168.69.121", 7624)
    if (not(indiclient.connectServer())):
         print("No indiserver running on " + indiclient.getHost() + ":" + str(indiclient.getPort()) + " - Try to run")
         print("  indiserver indi_simulator_telescope indi_simulator_ccd")
         sys.exit(1)


    # Wait for device to show up.
    device = None
    while True:
        device = indiclient.getDevice(RPI_CAMERA)
        if device:
            break
        print("Still waiting for device")
        time.sleep(1)

    # Wait for DEBUG property and and set to on.
    setSwitch(indiclient, 'DEBUG', 0, PyIndi.ISS_ON)
    setSwitch(indiclient, 'DEBUG_LEVEL', 0, 1)
    setSwitch(indiclient, 'DEBUG_LEVEL', 1, 1)
    setSwitch(indiclient, 'DEBUG_LEVEL', 2, 1)
    setSwitch(indiclient, 'DEBUG_LEVEL', 3, 1)
    # LOG_OUTPUT)=None(SWITCH) <- What should this be???

#    # Wait for CCD_INFO property
#    while True:
#        if device.getSwitch('CCD_INFO'):
#            break
#        print("Still waiting for CCD_INFO...")
#        time.sleep(1)


    # Wait for CONNECTION property and and set to on.
    setSwitch(indiclient, 'CONNECTION', 0, PyIndi.ISS_ON)

    # Make sure all other properties needed are there.
    while True:
        p = None
        for p in ["CCD_INFO", "CCD_GAIN", "CCD_EXPOSURE"]:
            prop = device.getProperty(p)
            if prop is None:
                break
        else:
            print("Got all necessary properties...")
            break

        print("Still waiting for {}".format(p))
        time.sleep(1)
    print("Found all properties...")


    connection = device.getSwitch("CONNECTION")
    connection[0].s = True
    indiclient.sendNewSwitch(connection)
    gain = device.getNumber("CCD_GAIN")
    assert(gain)
    gain[0].v = 1
    indiclient.sendNewNumber(gain)

    print("Taking picture")
    indiclient.takeExposure(2)
    while not indiclient.done:
        time.sleep(1)

    indiclient.disconnectServer()
