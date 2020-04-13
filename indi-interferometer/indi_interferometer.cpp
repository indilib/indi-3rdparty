/*
   INDI Driver for i-Nova PLX series
   Copyright 2013/2014 i-Nova Technologies - Ilia Platone

   Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)
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
    int len            = FRAME_SIZE;
    int olen           = 0;
    unsigned long counts[NUM_NODES];
    unsigned long correlations[NUM_BASELINES];
    char *buf = static_cast<char *>(malloc(len));
    double* framebuffer = static_cast<double*>(static_cast<void*>(PrimaryCCD.getFrameBuffer()));
    char str[3];
    while (InExposure)
    {
        tty_read_section(PortFD, buf, 13, 1000, &olen);
        if(olen != len)
            continue;
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
                framebuffer[static_cast<int>(center+w*uv.u+h*w*uv.v)] += static_cast<double>(correlations[idx++]*2.0/static_cast<double>(counts[x]+counts[y]));
            }
        }
    }
}

Interferometer::Interferometer()
{
    ExposureRequest = 0.0;
    InExposure = false;
}

bool Interferometer::Disconnect()
{
    return true;
}

const char * Interferometer::getDefaultName()
{
    return "Interferometer array";
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
    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "6.9f", 0.0000003, 1000000.0, 0.000000001, 0.0000004);
    IUFillNumber(&settingsN[1], "INTERFEROMETER_SAMPLERATE_VALUE", "Filter sample time (ns)", "9.0f", 20, 1000000.0, 20.0, 100.0);
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

    if (ccdConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]() { return callHandshake(); });
        registerConnection(serialConnection);
    }

    if (ccdConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]() { return callHandshake(); });

        registerConnection(tcpConnection);
    }

    // Set camera capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_HAS_DSP;
    SetCCDCapability(cap);
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
        defineNumber(locationNP);
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

        defineNumber(locationNP);
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

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void Interferometer::setupParams()
{
    SetCCDParams(PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), -64, getWavelength(), getWavelength());

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * abs(PrimaryCCD.getBPP()) / 8;
    nbuf += 512;  //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);
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
    int olen;
    int len = 2;
    char buf[2] = { 0x3c, 0x0d };
    tty_write(PortFD, buf, 2, &olen);

    // We're done
    return olen == len;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool Interferometer::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float Interferometer::CalcTimeLeft()
{
    float timesince;
    float timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (now.tv_sec * 1000.0f + now.tv_usec / 1000.0f) - (ExpStart.tv_sec * 1000.0f + ExpStart.tv_usec / 1000.0f);
    timesince = timesince / 1000.0f;

    timeleft = ExposureRequest - timesince;
    return timeleft;
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
        tty_write(PortFD, buf, len, &olen);
        IUUpdateNumber(&settingsNP, values, names, n);
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Client is asking us to set a new BLOB
***************************************************************************************/
bool Interferometer::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    return INDI::CCD::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
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
    float timeleft;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator for better timing checks
        if(timeleft >= 0.0f)
        {
            // Just update time left in client
            PrimaryCCD.setExposureLeft(static_cast<double>(timeleft));
        }
        else
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");
            // We're no longer exposing...
            InExposure = false;

            int olen;
            int len = 2;
            char buf[2] = { 0x0c, 0x0d };
            tty_write(PortFD, buf, 2, &olen);

            grabImage();
        }
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
    return false;
}

bool Interferometer::callHandshake()
{
    if (ccdConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}
