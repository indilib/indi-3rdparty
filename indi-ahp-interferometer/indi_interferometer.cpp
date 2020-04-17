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
#include <termios.h>
#include <unistd.h>
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
    double counts[NUM_NODES];
    double correlations[NUM_BASELINES];
    char buf[FRAME_SIZE+1];
    int w = PrimaryCCD.getXRes();
    int h = PrimaryCCD.getYRes();
    double *framebuffer = static_cast<double*>(malloc(w*h*sizeof(double)));
    memset(framebuffer, 0, w*h*sizeof(double));
    char str[SAMPLE_SIZE+1];
    str[SAMPLE_SIZE] = 0;
    tcflush(PortFD, TCIOFLUSH);
    while (InExposure)
    {
        tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if(olen != FRAME_SIZE)
            continue;
        timeleft -= FRAME_TIME;
        int idx = 0;
        int center = w*h/2;
        center += w/2;
        for(int x = NUM_NODES-1; x >= 0; x--) {
            strncpy(str, buf+idx, SAMPLE_SIZE);
            f_scansexa(str, &counts[x]);
            idx += SAMPLE_SIZE;
        }
        for(int x = NUM_BASELINES-1; x >= 0; x--) {
            strncpy(str, buf+idx, SAMPLE_SIZE);
            f_scansexa(str, &correlations[x]);
            idx += SAMPLE_SIZE;
        }
        idx = 0;
        for(int x = 0; x < NUM_NODES; x++) {
            for(int y = x+1; y < NUM_NODES; y++) {
                INDI::Correlator::UVCoordinate uv = baselines[idx]->getUVCoordinates();
                int xx = static_cast<int>(w*uv.u/2.0);
                int yy = static_cast<int>(h*uv.v/2.0);
                int z = center+xx+yy*w;
                if(xx >= -w/2 && xx < w/2 && yy >= -w/2 && yy < h/2) {
                    framebuffer[z] += correlations[idx]*65535.0/(counts[x]+counts[y]);
                    framebuffer[w*h-1-z] += correlations[idx]*65535.0/(counts[x]+counts[y]);
                }
                idx++;
            }
        }
        if(timeleft <= 0.0) {
            // We're no longer exposing...
            AbortExposure();
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");
            dsp_buffer_stretch(framebuffer, w*h, 0.0, 65535.0);
            dsp_buffer_copy(framebuffer, static_cast<unsigned short*>(static_cast<void*>(PrimaryCCD.getFrameBuffer())), w*h);
            // Let INDI::CCD know we're done filling the image buffer
            LOG_INFO("Download complete.");
            ExposureComplete(&PrimaryCCD);
        }
    }
    free (framebuffer);
}

Interferometer::Interferometer()
{
    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x] = new baseline();

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

bool Interferometer::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    for(int x = 0; x < NUM_NODES; x++)
        IUSaveConfigNumber(fp, &locationNP[x]);
    IUSaveConfigNumber(fp, &settingsNP);

    return true;
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
        sprintf(label, "Node %d", i);
        IUFillNumber(&locationN[i*3+0], "LOCATION_X", "Longitude offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);
        IUFillNumber(&locationN[i*3+1], "LOCATION_Y", "Latitude offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);
        IUFillNumber(&locationN[i*3+2], "LOCATION_Z", "Elevation offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);
        IUFillNumberVector(&locationNP[i], &locationN[i*3], 3, getDeviceName(), name, label, SITE_TAB, IP_RW, 60, IPS_IDLE);
    }
    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "%3.9f", 0.0000003, 999.0, 0.000000001, 21.112145);
    IUFillNumberVector(&settingsNP, settingsN, 1, getDeviceName(), "INTERFEROMETER_SETTINGS", "Interferometer Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

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

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_DSP;

    SetCCDCapability(cap);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 1.0, 3600.0, 1, false);

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
    SetCCDParams(2048, 2048, 16, 1, 1);

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
    char cmd[2] = { 0x3c, 0x0d };
    tcflush(PortFD, TCIOFLUSH);
    usleep(10000);
    tty_write(PortFD, &cmd[0], 1, &olen);
    usleep(10000);
    tty_write(PortFD, &cmd[1], 1, &olen);

    // We're done
    return olen == 1;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool Interferometer::AbortExposure()
{
    int olen = 0;
    char cmd[2] = { 0x0c, 0x0d };
    tcflush(PortFD, TCIOFLUSH);
    usleep(10000);
    tty_write(PortFD, &cmd[0], 1, &olen);
    usleep(10000);
    tty_write(PortFD, &cmd[1], 1, &olen);

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

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewNumber(dev, name, values, names, n);

    if(!strcmp(settingsNP.name, name)) {
        IUUpdateNumber(&settingsNP, values, names, n);
        for(int x = 0; x < NUM_BASELINES; x++)
            baselines[x]->setWavelength(settingsN[0].value);
        IDSetNumber(&settingsNP, nullptr);
        return true;
    }

    for (int i = 0; i < NUM_NODES; i++) {
        if(!strcmp(locationNP[i].name, name)) {
            IUUpdateNumber(&locationNP[i], values, names, n);
            locationN[i*3+0].value = values[0];
            locationN[i*3+1].value = values[1];
            locationN[i*3+2].value = values[2];
            int idx = 0;
            for(int x = 0; x < NUM_NODES; x++) {
                for(int y = x+1; y < NUM_NODES; y++) {
                    if(x==i||y==i) {
                        double x1,x2,y1,y2,z1,z2;
                        x1 = locationN[x*3+0].value;
                        x2 = locationN[y*3+0].value;
                        y1 = locationN[x*3+1].value;
                        y2 = locationN[y*3+1].value;
                        z1 = locationN[x*3+2].value;
                        z2 = locationN[y*3+2].value;

                        double Lat;

                        INumberVectorProperty *nv = this->getNumber("GEOGRAPHIC_COORDS");
                        if(nv != nullptr)
                        {
                            Lat = nv->np[0].value;
                        }
                        Lat *= M_PI/180.0;

                        INDI::Correlator::Baseline b;
                        b.x = (locationN[x*3+0].value-locationN[y*3+0].value);
                        b.y = (locationN[x*3+1].value-locationN[y*3+1].value)*sin(Lat);
                        b.z = (locationN[x*3+2].value-locationN[y*3+2].value)*cos(Lat);
                        baselines[idx]->setBaseline(b);
                    }
                    idx++;
                }
            }
            IDSetNumber(&locationNP[i], nullptr);
            return true;
        }
    }

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

bool Interferometer::Handshake()
{
    int ret = false;
    if(PortFD != -1) {
        int olen;
        char buf[FRAME_SIZE];

        char cmd[2] = { 0x3c, 0x0d };
        int ntries = 10;
        tcflush(PortFD, TCIOFLUSH);
        usleep(10000);
        tty_write(PortFD, &cmd[0], 1, &olen);
        usleep(10000);
        tty_write(PortFD, &cmd[1], 1, &olen);

        ntries = 10;
        while (olen != FRAME_SIZE && ntries-- > 0)
            tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if(olen == FRAME_SIZE)
            ret = true;

        cmd[0] = 0x0c;
        ntries = 10;
        tcflush(PortFD, TCIOFLUSH);
        usleep(10000);
        tty_write(PortFD, &cmd[0], 1, &olen);
        usleep(10000);
        tty_write(PortFD, &cmd[1], 1, &olen);
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
