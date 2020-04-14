/*
    indi_interferometer - a telescope array driver for INDI
    Support for AHP cross-correlators
    Copyright (C) 2020  Ilia Platone

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <sys/file.h>
#include <memory>
#include "indicom.h"
#include "indi_interferometer.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

static std::unique_ptr<Interferometer> array(new Interferometer());

void ISGetProperties(const char *dev)
{
    array->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    array->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(    const char *dev, const char *name, char *texts[], char *names[], int num)
{
    array->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    array->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    array->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    array->ISSnoopDevice(root);
}

void Interferometer::Callback()
{
    int olen           = 0;
    unsigned long counts[NUM_NODES];
    unsigned long correlations[NUM_BASELINES];
    char *buf = static_cast<char *>(malloc(FRAME_SIZE+1));
    unsigned short* framebuffer = static_cast<unsigned short*>(static_cast<void*>(PrimaryCCD.getFrameBuffer()));
    char str[3];
    while (InExposure)
    {
        tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if(olen != FRAME_SIZE)
            continue;
        timeleft -= FRAME_TIME_NS;
        int idx = 0;
        int w = PrimaryCCD.getSubW();
        int h = PrimaryCCD.getSubH();
        int center = w*h/2;
        for(int x = 0; x < NUM_NODES; x++) {
            strncpy(str, buf+idx, 3);
            counts[x] = strtoul(str, NULL, 16);
            idx += 3;
        }
        for(int x = 0; x < NUM_BASELINES; x++) {
            strncpy(str, buf+idx, 3);
            correlations[x] = strtoul(str, NULL, 16);
            idx += 3;
        }
        idx = 0;
        for(int x = 0; x < NUM_NODES; x++) {
            for(int y = x+1; y < NUM_NODES; y++) {
                INDI::Correlator::UVCoordinate uv = baselines[idx]->getUVCoordinates();
                int z = static_cast<int>(center+w*uv.u+h*w*uv.v);
                if(z > 0 && z < w*h)
                    framebuffer[z] = fmin(65535, framebuffer[z]+correlations[idx++]*4095/counts[x]+counts[y]);
            }
        }
        if(timeleft <= 0.0) {
            // We're no longer exposing...
            AbortExposure();
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");
            grabImage();
        }
    }
}

Interferometer::Interferometer()
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x] = new baseline();

    // Set camera capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_HAS_DSP;
    SetCCDCapability(cap);
    setInterferometerConnection(CONNECTION_TCP|CONNECTION_SERIAL);

    ExposureRequest = 0.0;
    InExposure = false;
}

bool Interferometer::Disconnect()
{
    return true;
}

const char * Interferometer::getDefaultName()
{
    return "Telescope array";
}

const char * Interferometer::getDeviceName()
{
    return getDefaultName();
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool Interferometer::initProperties()
{
    // Must init parent properties first!
    INDI::CCD::initProperties();

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->initProperties();

    char name[MAXINDINAME];
    char label[MAXINDILABEL];
    for (int i = 0; i < NUM_NODES; i++) {
        sprintf(name, "LOCATION_NODE%02d", i);
        sprintf(label, "Node %d location", i);
        IUFillNumber(&locationN[i*3+0], "LOCATION_X", "X", "%4.1f", 0.75, 9999.0, 0.75, 10.0);
        IUFillNumber(&locationN[i*3+1], "LOCATION_Y", "Y", "%4.1f", 0.75, 9999.0, 0.75, 10.0);
        IUFillNumber(&locationN[i*3+2], "LOCATION_Z", "Z", "%4.1f", 0.75, 9999.0, 0.75, 10.0);
        IUFillNumberVector(&locationNP[i], &locationN[i*3], 3, getDeviceName(), name, label, INTERFEROMETER_PROPERTIES_TAB, IP_RW, 60, IPS_IDLE);
    }
    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "%6.9f", 0.0000003, 1000000.0, 0.000000001, 0.0000004);
    IUFillNumber(&settingsN[1], "INTERFEROMETER_SAMPLERATE_VALUE", "Filter sample time (ns)", "%9.0f", 20, 1000000.0, 20.0, 100.0);
    IUFillNumberVector(&settingsNP, settingsN, 2, getDeviceName(), "INTERFEROMETER_SETTINGS", "Interferometer Settings", INTERFEROMETER_PROPERTIES_TAB, IP_RW, 60, IPS_IDLE);

    int idx = 0;
    for(int x = 0; x < NUM_NODES; x++) {
        for(int y = x+1; y < NUM_NODES; y++) {
            INDI::Correlator::Baseline b;
            b.x = locationN[x*3+0].value-locationN[y*3+0].value;
            b.y = locationN[x*3+1].value-locationN[y*3+1].value;
            b.z = locationN[x*3+2].value-locationN[y*3+2].value;
            baselines[idx]->setBaseline(b);
            idx++;
        }
    }

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.0001, 30000000.0, 1, false);

    setDefaultPollingPeriod(500);

    if (interferometerConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]() { return callHandshake(); });
        registerConnection(serialConnection);
    }

    if (interferometerConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]() { return callHandshake(); });

        registerConnection(tcpConnection);
    }
    return true;

}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void Interferometer::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    if (isConnected())
    {
        for (int x=0; x<NUM_NODES; x++)
            defineNumber(&locationNP[x]);
        defineNumber(&settingsNP);
        // Define our properties
    }
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool Interferometer::updateProperties()
{
    // Call parent update properties
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // Let's get parameters now from CCD
        setupParams();

        for (int x=0; x<NUM_NODES; x++)
            defineNumber(&locationNP[x]);
        defineNumber(&settingsNP);
        // Start the timer
        SetTimer(POLLMS);
    }
    else
        // We're disconnected
    {
        deleteProperty(settingsNP.name);
        for (int x=0; x<NUM_NODES; x++)
            deleteProperty(locationNP[x].name);
    }

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->updateProperties();
    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void Interferometer::setupParams()
{
    SetCCDParams(2048, 2048, 16, AIRY*RAD_AS*getWavelength(), AIRY*RAD_AS*getWavelength());

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    nbuf += 512;  //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);
    memset(PrimaryCCD.getFrameBuffer(), 0, PrimaryCCD.getFrameBufferSize());
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool Interferometer::StartExposure(float duration)
{
    ExposureRequest = duration;
    PrimaryCCD.setExposureDuration(static_cast<double>(ExposureRequest));
    std::thread(&Interferometer::Callback, this).detach();
    gettimeofday(&ExpStart, nullptr);
    InExposure = true;
    timeleft = ExposureRequest;

    int olen;
    int len = 2;
    char buf[2] = { 0x3c, 0x0d };
    int ntries = 10;
    while (olen != len && ntries-- > 0)
        tty_write(PortFD, buf, len, &olen);

    // We're done
    return olen == len;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool Interferometer::AbortExposure()
{
    int olen = 0;
    int len = 2;
    char buf[2] = { 0x0c, 0x0d };
    int ntries = 10;
    while (olen != len && ntries-- > 0)
        tty_write(PortFD, buf, len, &olen);
    InExposure = false;
    return true;
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool Interferometer::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    for (int i = 0; i < NUM_NODES; i++) {
        if(!strcmp(locationNP[i].name, name)) {
            locationN[i*3+0].value = values[0];
            locationN[i*3+1].value = values[1];
            locationN[i*3+2].value = values[2];
            int idx = 0;
            for(int x = 0; x < NUM_NODES; x++) {
                for(int y = x+1; y < NUM_NODES; y++) {
                    if(x==i||y==i) {
                        INDI::Correlator::Baseline b;
                        b.x = locationN[x*3+0].value-locationN[y*3+0].value;
                        b.y = locationN[x*3+1].value-locationN[y*3+1].value;
                        b.z = locationN[x*3+2].value-locationN[y*3+2].value;
                        baselines[idx]->setBaseline(b);
                    }
                    idx++;
                }
            }
            IUUpdateNumber(&locationNP[i], values, names, n);
        }
    }
    if(!strcmp(settingsNP.name, name)) {
        setWavelength(values[0]);
        int len = 16;
        int olen;
        char buf[17];
        unsigned long value = static_cast<unsigned long>(values[1]);
        for(int i = 0; i < 16; i++) {
            buf[i] = (value&0xf)<<4 | 0x01;
            value>>=4;
        }
        buf[16] = '\r';
        int ntries = 10;
        while (olen != len && ntries-- > 0)
            tty_write(PortFD, buf, len, &olen);
        IUUpdateNumber(&settingsNP, values, names, n);
    }

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewNumber(dev, name, values, names, n);
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Client is asking us to set a new switch
***************************************************************************************/
bool Interferometer::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewSwitch(dev, name, states, names, n);
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Client is asking us to set a new text
***************************************************************************************/
bool Interferometer::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewText(dev, name, texts, names, n);
    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Client is asking us to set a new BLOB
***************************************************************************************/
bool Interferometer::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
    return INDI::CCD::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Client is asking us to set a new snoop device
***************************************************************************************/
bool Interferometer::ISSnoopDevice(XMLEle *root)
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISSnoopDevice(root);
    return INDI::CCD::ISSnoopDevice(root);
}

/**************************************************************************************
** INDI is asking us to add any FITS keywords to the FITS header
***************************************************************************************/
void Interferometer::addFITSKeywords(fitsfile *fptr, INDI::CCDChip *targetChip)
{
    // Let's first add parent keywords
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // Add temperature to FITS header
    int status = 0;
    fits_write_date(fptr, &status);

}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void Interferometer::TimerHit()
{
    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        // Just update time left in client
        PrimaryCCD.setExposureLeft(static_cast<double>(timeleft));
    }

    SetTimer(POLLMS);
    return;
}

void Interferometer::grabImage()
{
    std::unique_lock<std::mutex> guard(ccdBufferLock);
    // Let's get a pointer to the frame buffer
    unsigned char * image = PrimaryCCD.getFrameBuffer();
    if(image != nullptr)
    {
        guard.unlock();
        // Let INDI::CCD know we're done filling the image buffer
        LOG_INFO("Download complete.");
        ExposureComplete(&PrimaryCCD);
    }
}

bool Interferometer::Handshake()
{
    int ret = false;
    if(PortFD != -1) {
        int olen;
        char buf[FRAME_SIZE];

        char cmd[2] = { 0x3c, 0x0d };
        int ntries = 10;
        while (olen != 2 && ntries-- > 0)
            tty_write(PortFD, cmd, 2, &olen);

        ntries = 10;
        while (olen != FRAME_SIZE && ntries-- > 0)
            tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if(olen == FRAME_SIZE)
            ret = true;

        DEBUGF(INDI::Logger::DBG_ERROR, "len=%d", olen);

        cmd[0] = 0x0c;
        ntries = 10;
        while (olen != 2 && ntries-- > 0)
            tty_write(PortFD, cmd, 2, &olen);

    }
    return ret;
}

bool Interferometer::callHandshake()
{
    if (interferometerConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

uint8_t Interferometer::getInterferometerConnection() const
{
    return interferometerConnection;
}

void Interferometer::setInterferometerConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    interferometerConnection = value;
}
