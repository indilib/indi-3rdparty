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
#include <indicom.h>
#include <connectionplugins/connectionserial.h>
#include "indi_interferometer.h"

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
    double counts[NUM_LINES];
    double correlations[NUM_BASELINES];
    char buf[FRAME_SIZE+1];
    int w = PrimaryCCD.getXRes();
    int h = PrimaryCCD.getYRes();
    double *framebuffer = static_cast<double*>(malloc(w*h*sizeof(double)));
    memset(framebuffer, 0, w*h*sizeof(double));
    char str[MAXINDINAME];
    str[SAMPLE_SIZE] = 0;

    tcflush(PortFD, TCIOFLUSH);
    EnableCapture(true);
    threadsRunning = true;
    while (threadsRunning)
    {
        tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if (olen != FRAME_SIZE) {
            continue;
        }
        if(InExposure) {
            timeleft -= FRAME_TIME;
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
        memset(counts, 0, NUM_LINES*sizeof(double));
        memset(correlations, 0, NUM_BASELINES*sizeof(double));
        int idx = HEADER_SIZE;
        int center = w*h/2;
        center += w/2;
        unsigned int tmp;
        for(int x = NUM_LINES-1; x >= 0; x--) {
            memset(str, 0, SAMPLE_SIZE+1);
            strncpy(str, buf+idx, SAMPLE_SIZE);
            sscanf(str, "%X", &tmp);
            counts[x] = static_cast<double>(tmp);
            totalcounts[x] += counts[x];
            idx += SAMPLE_SIZE;
        }
        for(int x = NUM_BASELINES-1; x >= 0; x--) {
            for(int y = 0; y < DELAY_LINES; y++) {
                memset(str, 0, SAMPLE_SIZE+1);
                strncpy(str, buf+idx, SAMPLE_SIZE);
                sscanf(str, "%X", &tmp);
                correlations[x] += static_cast<double>(tmp)/(fabs(y-DELAY_LINES/2)+1);
                idx += SAMPLE_SIZE;
            }
            totalcorrelations[x] += correlations[x];
        }
        idx = 0;
        double maxalt = 0;
        int closest = 0;
        for(int x = 0; x < NUM_LINES; x++) {
            for(int y = x+1; y < NUM_LINES; y++) {
                if(lineEnableSP[x].sp[0].s == ISS_ON) {
                    INDI::Correlator::UVCoordinate uv = baselines[idx]->getUVCoordinates();
                    int xx = static_cast<int>(w*uv.u/2.0);
                    int yy = static_cast<int>(h*uv.v/2.0);
                    int z = center+xx+yy*w;
                    if(xx >= -w/2 && xx < w/2 && yy >= -w/2 && yy < h/2) {
                        framebuffer[z] += correlations[idx]*2.0/(counts[x]+counts[y]);
                        framebuffer[w*h-1-z] += correlations[idx]*2.0/(counts[x]+counts[y]);
                    }
                    double lst = get_local_sidereal_time(lineGPSNP[x].np[1].value);
                    double ha = get_local_hour_angle(lst, lineTelescopeNP[x].np[0].value);
                    get_alt_az_coordinates(ha, lineTelescopeNP[x].np[1].value, lineGPSNP[x].np[0].value, &alt[x], &az[x]);
                    closest = (maxalt > alt[x] ? closest : x);
                    maxalt = (maxalt > alt[x] ? maxalt : alt[x]);
                }
                idx++;
            }
        }
        delay[closest] = 0;
        idx = 0;
        for(int x = 0; x < NUM_LINES; x++) {
            for(int y = x+1; y < NUM_LINES; y++) {
                INDI::Correlator::Baseline b = baselines[idx]->getBaseline();
                double d = sqrt(pow(b.x, 2) + pow(b.y, 2) + pow(b.z, 2));
                idx++;
                double t = maxalt*M_PI/180.0;
                if(x == closest) {
                    t -=  alt[y]*M_PI/180.0;
                    delay[y] = d * cos(t);
                }
                if(y == closest) {
                    t -=  alt[x]*M_PI/180.0;
                    delay[x] = d * cos(t);
                }
            }
        }
        for(int x = 0; x < NUM_LINES; x++) {
            unsigned int delay_clocks = delay[x] * clock_frequency / LIGHTSPEED;
            SendCommand(SET_ACTIVE_LINE, x);
            SendCommand(CLEAR, 0);
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(SET_DELAY, delay_clocks&0xf);
            delay_clocks >>= 4;
            SendCommand(COMMIT, 0);
        }
    }
    EnableCapture(false);
    free (framebuffer);
}

Interferometer::Interferometer()
{
    clock_frequency = 0;

    ExposureRequest = 0.0;
    InExposure = false;

    SAMPLE_SIZE = 0;
    NUM_LINES = 0;
    DELAY_LINES = 0;

    lineStatsN = static_cast<INumber*>(malloc(1));
    lineStatsNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineEnableS = static_cast<ISwitch*>(malloc(1));
    lineEnableSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    linePowerS = static_cast<ISwitch*>(malloc(1));
    linePowerSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    lineDevicesT = static_cast<IText*>(malloc(1));
    lineDevicesTP = static_cast<ITextVectorProperty*>(malloc(1));

    snoopGPSN = static_cast<INumber*>(malloc(1));
    snoopGPSNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopTelescopeN = static_cast<INumber*>(malloc(1));
    snoopTelescopeNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopTelescopeInfoN = static_cast<INumber*>(malloc(1));
    snoopTelescopeInfoNP = static_cast<INumberVectorProperty*>(malloc(1));

    snoopDomeN = static_cast<INumber*>(malloc(1));
    snoopDomeNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineDelayN = static_cast<INumber*>(malloc(1));
    lineDelayNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineGPSN = static_cast<INumber*>(malloc(1));
    lineGPSNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineTelescopeN = static_cast<INumber*>(malloc(1));
    lineTelescopeNP = static_cast<INumberVectorProperty*>(malloc(1));

    lineDomeN = static_cast<INumber*>(malloc(1));
    lineDomeNP = static_cast<INumberVectorProperty*>(malloc(1));

    correlationsN = static_cast<INumber*>(malloc(1));

    totalcounts = static_cast<double*>(malloc(1));
    totalcorrelations = static_cast<double*>(malloc(1));
    alt = static_cast<double*>(malloc(1));
    az = static_cast<double*>(malloc(1));
    delay = static_cast<double*>(malloc(1));
    baselines = static_cast<baseline**>(malloc(1));
}

bool Interferometer::Disconnect()
{
    for(int x = 0; x < NUM_LINES; x++)
        ActiveLine(x, false, false);

    threadsRunning = false;
    usleep(1000000);

    return INDI::CCD::Disconnect();
}

const char * Interferometer::getDefaultName()
{
    return "AHP Telescope array correlator";
}

const char * Interferometer::getDeviceName()
{
    return getDefaultName();
}

bool Interferometer::saveConfigItems(FILE *fp)
{
    for(int x = 0; x < NUM_LINES; x++) {
        IUSaveConfigSwitch(fp, &lineEnableSP[x]);
        if(lineEnableSP[x].sp[0].s == ISS_ON) {
            IUSaveConfigText(fp, &lineDevicesTP[x]);
            IUSaveConfigSwitch(fp, &linePowerSP[x]);
        }
    }
    IUSaveConfigNumber(fp, &settingsNP);

    INDI::CCD::saveConfigItems(fp);
    return true;
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool Interferometer::initProperties()
{

    // Must init parent properties first!
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT|CCD_CAN_SUBFRAME|CCD_HAS_DSP);

    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "%g", 3.0E-12, 3.0E+3, 1.0E-9, 0.211121449);
    IUFillNumber(&settingsN[1], "INTERFEROMETER_BANDWIDTH_VALUE", "Filter bandwidth (m)", "%g", 3.0E-12, 3.0E+3, 1.0E-9, 1199.169832);
    IUFillNumberVector(&settingsNP, settingsN, 2, getDeviceName(), "INTERFEROMETER_SETTINGS", "Interferometer Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 1.0, STELLAR_DAY, 1, false);
    setDefaultPollingPeriod(500);

    serialConnection = new Connection::Serial(this);
    serialConnection->setWordSize(WORD_SIZE);
    serialConnection->setStopBits(STOP_BITS);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);

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
        for (int x=0; x<NUM_LINES; x++) {
            defineSwitch(&lineEnableSP[x]);
        }
        defineNumber(&correlationsNP);
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

        for (int x=0; x<NUM_LINES; x++) {
            defineSwitch(&lineEnableSP[x]);
        }
        defineNumber(&correlationsNP);
        defineNumber(&settingsNP);
    }
    else
        // We're disconnected
    {
        deleteProperty(correlationsNP.name);
        deleteProperty(settingsNP.name);
        for (int x=0; x<NUM_LINES; x++) {
            deleteProperty(lineEnableSP[x].name);
            deleteProperty(linePowerSP[x].name);
            deleteProperty(lineGPSNP[x].name);
            deleteProperty(lineTelescopeNP[x].name);
            deleteProperty(lineStatsNP[x].name);
            deleteProperty(lineDevicesTP[x].name);
            deleteProperty(lineDelayNP[x].name);
        }
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
    SetCCDParams(MAX_RESOLUTION, MAX_RESOLUTION, 16,  PIXEL_SIZE, PIXEL_SIZE);

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
    if(InExposure)
        return false;

    ExposureRequest = duration;
    timeleft = ExposureRequest;
    PrimaryCCD.setExposureDuration(static_cast<double>(ExposureRequest));

    InExposure = true;
    // We're done
    return true;
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
        for(int x = 0; x < NUM_BASELINES; x++) {
            baselines[x]->setWavelength(settingsN[0].value);
        }
        IDSetNumber(&settingsNP, nullptr);
        return true;
    }
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Client is asking us to set a new switch
***************************************************************************************/
bool Interferometer::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    if(!strcmp(name, "DEVICE_BAUD_RATE")) {
        if(states[0] == ISS_ON || states[1] == ISS_ON || states[2] == ISS_ON) {
            states[0] = states[1] = states[2] = ISS_OFF;
            states[3] = ISS_ON;
        }
        IUUpdateSwitch(getSwitch("DEVICE_BAUD_RATE"), states, names, n);
        if (states[3] == ISS_ON) {
            SendCommand(SET_BAUDRATE, 0);
        }
        if (states[4] == ISS_ON) {
            SendCommand(SET_BAUDRATE, 1);
        }
        if (states[5] == ISS_ON) {
            SendCommand(SET_BAUDRATE, 2);
        }
        IDSetSwitch(getSwitch("DEVICE_BAUD_RATE"), nullptr);
    }

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewSwitch(dev, name, states, names, n);

    for(int x = 0; x < NUM_LINES; x++) {
        if(!strcmp(name, lineEnableSP[x].name)){
            IUUpdateSwitch(&lineEnableSP[x], states, names, n);
            if(lineEnableSP[x].sp[0].s == ISS_ON) {
                ActiveLine(x, true, linePowerSP[x].sp[0].s == ISS_ON);
                defineSwitch(&linePowerSP[x]);
                defineNumber(&lineGPSNP[x]);
                defineNumber(&lineTelescopeNP[x]);
                defineNumber(&lineDelayNP[x]);
                defineNumber(&lineStatsNP[x]);
                defineText(&lineDevicesTP[x]);
            } else {
                ActiveLine(x, false, false);
                deleteProperty(linePowerSP[x].name);
                deleteProperty(lineGPSNP[x].name);
                deleteProperty(lineTelescopeNP[x].name);
                deleteProperty(lineStatsNP[x].name);
                deleteProperty(lineDevicesTP[x].name);
                deleteProperty(lineDelayNP[x].name);
            }
            IDSetSwitch(&lineEnableSP[x], nullptr);
        }
        if(!strcmp(name, linePowerSP[x].name)){
            IUUpdateSwitch(&linePowerSP[x], states, names, n);
            ActiveLine(x, true, linePowerSP[x].sp[0].s == ISS_ON);
            IDSetSwitch(&linePowerSP[x], nullptr);
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Client is asking us to set a new BLOB
***************************************************************************************/
bool Interferometer::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);

    return INDI::CCD::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Client is asking us to set a new text
***************************************************************************************/
bool Interferometer::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    //  This is for our device
    //  Now lets see if it's something we process here
    for(int x = 0; x < NUM_LINES; x++) {
        if (!strcmp(name, lineDevicesTP[x].name))
        {
            lineDevicesTP[x].s = IPS_OK;
            IUUpdateText(&lineDevicesTP[x], texts, names, n);
            IDSetText(&lineDevicesTP[x], nullptr);

            // Update the property name!
            strncpy(snoopTelescopeNP[x].device, lineDevicesTP[x].tp[0].text, MAXINDIDEVICE);
            strncpy(snoopTelescopeInfoNP[x].device, lineDevicesTP[x].tp[0].text, MAXINDIDEVICE);
            strncpy(snoopGPSNP[x].device, lineDevicesTP[x].tp[1].text, MAXINDIDEVICE);
            strncpy(snoopDomeNP[x].device, lineDevicesTP[x].tp[2].text, MAXINDIDEVICE);

            IDSnoopDevice(lineDevicesTP[x].tp[0].text, "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(lineDevicesTP[x].tp[0].text, "TELESCOPE_INFO");
            IDSnoopDevice(snoopGPSNP[x].device, "GEOGRAPHIC_COORD");
            IDSnoopDevice(snoopDomeNP[x].device, "GEOGRAPHIC_COORD");

            //  We processed this one, so, tell the world we did it
            return true;
        }
    }

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewText(dev, name, texts, names, n);

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Client is asking us to set a new snoop device
***************************************************************************************/
bool Interferometer::ISSnoopDevice(XMLEle *root)
{
    for(int i = 0; i < NUM_LINES; i++) {
        if(!IUSnoopNumber(root, &snoopTelescopeNP[i])) {
            lineTelescopeNP[i].s = IPS_BUSY;
            lineTelescopeNP[i].np[0].value = snoopTelescopeNP[i].np[0].value;
            lineTelescopeNP[i].np[1].value = snoopTelescopeNP[i].np[1].value;
            IDSetNumber(&lineTelescopeNP[i], nullptr);
        }
        if(!IUSnoopNumber(root, &snoopTelescopeInfoNP[i])) {
            lineTelescopeNP[i].s = IPS_BUSY;
            lineTelescopeNP[i].np[2].value = snoopTelescopeInfoNP[i].np[0].value;
            lineTelescopeNP[i].np[3].value = snoopTelescopeInfoNP[i].np[1].value;
            IDSetNumber(&lineTelescopeNP[i], nullptr);
        }
        if(!IUSnoopNumber(root, &snoopGPSNP[i])) {
            lineGPSNP[i].s = IPS_BUSY;
            double Lat0, Lon0;
            lineGPSNP[i].np[0].value = snoopGPSNP[i].np[0].value;
            lineGPSNP[i].np[1].value = snoopGPSNP[i].np[1].value;
            lineGPSNP[i].np[2].value = snoopGPSNP[i].np[2].value;
            int idx = 0;
            for(int x = 0; x < NUM_LINES; x++) {
                for(int y = x+1; y < NUM_LINES; y++) {
                    if(x==i||y==i) {
                        double Lat1, Lon1;
                        Lat0 = snoopGPSNP[x].np[0].value*M_PI/180.0;
                        Lon0 = snoopGPSNP[x].np[1].value*M_PI/180.0;
                        Lat1 = snoopGPSNP[y].np[0].value*M_PI/180.0;
                        Lon1 = snoopGPSNP[y].np[1].value*M_PI/180.0;
                        double x0 = cos(Lat0)*cos(Lon0)*(EARTHRADIUSMEAN+snoopGPSNP[x].np[2].value);
                        double y0 = cos(Lat0)*sin(Lon0)*(EARTHRADIUSMEAN+snoopGPSNP[x].np[2].value);
                        double z0 = sin(Lat0)*(EARTHRADIUSMEAN+snoopGPSNP[x].np[2].value);
                        double x1 = cos(Lat1)*cos(Lon1)*(EARTHRADIUSMEAN+snoopGPSNP[y].np[2].value);
                        double y1 = cos(Lat1)*sin(Lon1)*(EARTHRADIUSMEAN+snoopGPSNP[y].np[2].value);
                        double z1 = sin(Lat1)*(EARTHRADIUSMEAN+snoopGPSNP[y].np[2].value);
                        INDI::Correlator::Baseline b;
                        b.x = (x0-x1);
                        b.y = (y0-y1);
                        b.z = (z0-z1);
                        baselines[idx]->setBaseline(b);
                    }
                    idx++;
                }
            }
            IDSetNumber(&lineGPSNP[i], nullptr);
        }
    }

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
    if(!isConnected())
        return;  //  No need to reset timer if we are not connected anymore

    if(InExposure) {
        // Just update time left in client
        PrimaryCCD.setExposureLeft(static_cast<double>(timeleft));
    }

    IDSetNumber(&correlationsNP, nullptr);
    int idx = 0;
    for (int x = 0; x < NUM_LINES; x++) {
        lineDelayNP[x].s = IPS_BUSY;
        lineDelayNP[x].np[0].value = delay[x];
        IDSetNumber(&lineDelayNP[x], nullptr);
        lineStatsNP[x].s = IPS_BUSY;
        lineStatsNP[x].np[0].value = totalcounts[x];
        IDSetNumber(&lineStatsNP[x], nullptr);
        double steradian = pow(asin(lineTelescopeNP[x].np[2].value*0.5/lineTelescopeNP[x].np[3].value), 2);
        double photon_flux = totalcounts[x]*1000.0/POLLMS;
        double photon_flux0 = calc_photon_flux(0, settingsNP.np[1].value, settingsNP.np[0].value, steradian);
        lineStatsNP[x].np[1].value = photon_flux/LUMEN(settingsNP.np[0].value);
        lineStatsNP[x].np[2].value = photon_flux0/LUMEN(settingsNP.np[0].value);
        lineStatsNP[x].np[3].value = calc_rel_magnitude(photon_flux, settingsNP.np[1].value, settingsNP.np[0].value, steradian);
        for(int y = x+1; y < NUM_LINES; y++) {
            correlationsNP.np[idx*2+0].value = totalcorrelations[idx];
            correlationsNP.np[idx*2+1].value = totalcorrelations[idx]*2.0/(totalcounts[x]+totalcounts[y]);
            totalcorrelations[idx] = 0;
            idx++;
        }
        totalcounts[x] = 0;
    }

    SetTimer(POLLMS);

    return;
}

bool Interferometer::Handshake()
{
    PortFD = serialConnection->getPortFD();
    if(PortFD != -1) {
        int tmp = 0;
        int sample_size = 0;
        int num_lines = 0;
        int delay_lines = 0;

        int olen;
        char buf[HEADER_SIZE];

        EnableCapture(true);

        while (buf[0] != 0xd) {
            int ntries = 10;
            while (ntries-- > 0)
                tty_read(PortFD, buf, 1, 1, &olen);
            if(ntries == 0 || olen != 1) {
                SAMPLE_SIZE = 0;
                NUM_LINES = 0;
                DELAY_LINES = 0;
                return false;
            }
        }

        tty_read(PortFD, buf, HEADER_SIZE, 1, &olen);
        if(olen != HEADER_SIZE) {
            SAMPLE_SIZE = 0;
            NUM_LINES = 0;
            DELAY_LINES = 0;
            return false;
        }

        sscanf(buf, "%02X%02X%02X%02X%08X", &tmp, &sample_size, &num_lines, &delay_lines, &clock_frequency);

        for(int x = 0; x < NUM_LINES; x++) {
            if(baselines[x] != nullptr) {
                baselines[x]->~baseline();
            }
        }

        lineStatsN = static_cast<INumber*>(realloc(lineStatsN, 4*num_lines*sizeof(INumber)+1));
        lineStatsNP = static_cast<INumberVectorProperty*>(realloc(lineStatsNP, num_lines*sizeof(INumberVectorProperty)+1));

        lineEnableS = static_cast<ISwitch*>(realloc(lineEnableS, num_lines*2*sizeof(ISwitch)));
        lineEnableSP = static_cast<ISwitchVectorProperty*>(realloc(lineEnableSP, num_lines*sizeof(ISwitchVectorProperty)+1));

        linePowerS = static_cast<ISwitch*>(realloc(linePowerS, num_lines*2*sizeof(ISwitch)+1));
        linePowerSP = static_cast<ISwitchVectorProperty*>(realloc(linePowerSP, num_lines*sizeof(ISwitchVectorProperty)+1));

        lineDevicesT = static_cast<IText*>(realloc(lineDevicesT, 3*num_lines*sizeof(IText)+1));
        lineDevicesTP = static_cast<ITextVectorProperty*>(realloc(lineDevicesTP, num_lines*sizeof(ITextVectorProperty)+1));

        lineGPSN = static_cast<INumber*>(realloc(lineGPSN, 3*num_lines*sizeof(INumber)+1));
        lineGPSNP = static_cast<INumberVectorProperty*>(realloc(lineGPSNP, num_lines*sizeof(INumberVectorProperty)+1));

        lineTelescopeN = static_cast<INumber*>(realloc(lineTelescopeN, 4*num_lines*sizeof(INumber)+1));
        lineTelescopeNP = static_cast<INumberVectorProperty*>(realloc(lineTelescopeNP, num_lines*sizeof(INumberVectorProperty)+1));

        lineDomeN = static_cast<INumber*>(realloc(lineDomeN, 2*num_lines*sizeof(INumber)+1));
        lineDomeNP = static_cast<INumberVectorProperty*>(realloc(lineDomeNP, num_lines*sizeof(INumberVectorProperty)+1));

        snoopGPSN = static_cast<INumber*>(realloc(snoopGPSN, 3*num_lines*sizeof(INumber)+1));
        snoopGPSNP = static_cast<INumberVectorProperty*>(realloc(snoopGPSNP, num_lines*sizeof(INumberVectorProperty)+1));

        snoopTelescopeN = static_cast<INumber*>(realloc(snoopTelescopeN, 2*num_lines*sizeof(INumber)+1));
        snoopTelescopeNP = static_cast<INumberVectorProperty*>(realloc(snoopTelescopeNP, num_lines*sizeof(INumberVectorProperty)+1));

        snoopTelescopeInfoN = static_cast<INumber*>(realloc(snoopTelescopeInfoN, 4*num_lines*sizeof(INumber)+1));
        snoopTelescopeInfoNP = static_cast<INumberVectorProperty*>(realloc(snoopTelescopeInfoNP, num_lines*sizeof(INumberVectorProperty)+1));

        snoopDomeN = static_cast<INumber*>(realloc(snoopDomeN, 2*num_lines*sizeof(INumber)+1));
        snoopDomeNP = static_cast<INumberVectorProperty*>(realloc(snoopDomeNP, num_lines*sizeof(INumberVectorProperty)+1));

        lineDelayN = static_cast<INumber*>(realloc(lineDelayN, num_lines*sizeof(INumber)+1));
        lineDelayNP = static_cast<INumberVectorProperty*>(realloc(lineDelayNP, num_lines*sizeof(INumberVectorProperty)+1));

        correlationsN = static_cast<INumber*>(realloc(correlationsN, 2*num_lines*(num_lines-1)/2*sizeof(INumber)+1));

        totalcounts = static_cast<double*>(realloc(totalcounts, num_lines*sizeof(double)+1));
        totalcorrelations = static_cast<double*>(realloc(totalcorrelations, (num_lines*(num_lines-1)/2)*sizeof(double)+1));
        alt = static_cast<double*>(realloc(alt, num_lines*sizeof(double)+1));
        az = static_cast<double*>(realloc(az, num_lines*sizeof(double)+1));
        delay = static_cast<double*>(realloc(delay, num_lines*sizeof(double)+1));
        baselines = static_cast<baseline**>(realloc(baselines, (num_lines*(num_lines-1)/2)*sizeof(baseline*)+1));

        memset (totalcounts, 0, num_lines*sizeof(double)+1);
        memset (totalcorrelations, 0, (num_lines*(num_lines-1)/2)*sizeof(double)+1);
        memset (alt, 0, num_lines*sizeof(double)+1);
        memset (az, 0, num_lines*sizeof(double)+1);

        for(int x = 0; x < (num_lines*(num_lines-1)/2); x++) {
            baselines[x] = new baseline();
            baselines[x]->initProperties();
        }

        int idx = 0;
        char tab[MAXINDINAME];
        char name[MAXINDINAME];
        char label[MAXINDINAME];
        for (int x = 0; x < num_lines; x++) {
            //snoop properties
            IUFillNumber(&snoopTelescopeN[x*2+0], "RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
            IUFillNumber(&snoopTelescopeN[x*2+1], "DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);

            IUFillNumber(&snoopTelescopeInfoN[x*4+0], "TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
            IUFillNumber(&snoopTelescopeInfoN[x*4+1], "TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);
            IUFillNumber(&snoopTelescopeInfoN[x*4+2], "GUIDER_APERTURE", "Guider Aperture (mm)", "%g", 10, 5000, 0, 0.0);
            IUFillNumber(&snoopTelescopeInfoN[x*4+3], "GUIDER_FOCAL_LENGTH", "Guider Focal Length (mm)", "%g", 10, 10000, 0, 0.0);

            IUFillNumber(&snoopGPSN[x*3+0], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
            IUFillNumber(&snoopGPSN[x*3+1], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
            IUFillNumber(&snoopGPSN[x*3+2], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);

            IUFillNumber(&lineDelayN[x], "DELAY", "Delay (m)", "%g", 0, EARTHRADIUSMEAN, 1.0E-9, 0);

            IUFillNumberVector(&snoopGPSNP[x], &snoopGPSN[x*3], 3, getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
            IUFillNumberVector(&snoopTelescopeNP[x], &snoopTelescopeN[x*2], 2, getDeviceName(), "EQUATORIAL_EOD_COORD", "Target coordinates", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
            IUFillNumberVector(&snoopTelescopeInfoNP[x], &snoopTelescopeInfoN[x*4], 4, getDeviceName(), "TELESCOPE_INFO", "Scope Properties", OPTIONS_TAB, IP_RW, 60, IPS_OK);

            lineDevicesT[x*3+0].text = static_cast<char*>(malloc(1));
            IUFillText(&lineDevicesT[x*3+0], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
            lineDevicesT[x*3+1].text = static_cast<char*>(malloc(1));
            IUFillText(&lineDevicesT[x*3+1], "ACTIVE_GPS", "GPS", "GPS Simulator");
            lineDevicesT[x*3+2].text = static_cast<char*>(malloc(1));
            IUFillText(&lineDevicesT[x*3+2], "ACTIVE_DOME", "DOME", "Dome Simulator");

            //interferometer properties
            IUFillNumber(&lineTelescopeN[x*4+0], "RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
            IUFillNumber(&lineTelescopeN[x*4+1], "DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
            IUFillNumber(&lineTelescopeN[x*4+2], "TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0.0);
            IUFillNumber(&lineTelescopeN[x*4+3], "TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0.0);

            IUFillNumber(&lineGPSN[x*3+0], "LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
            IUFillNumber(&lineGPSN[x*3+1], "LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
            IUFillNumber(&lineGPSN[x*3+2], "ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);

            IUFillSwitch(&lineEnableS[x*2+0], "LINE_ENABLE", "Enable", ISS_OFF);
            IUFillSwitch(&lineEnableS[x*2+1], "LINE_DISABLE", "Disable", ISS_ON);

            IUFillSwitch(&linePowerS[x*2+0], "LINE_POWER_ON", "On", ISS_OFF);
            IUFillSwitch(&linePowerS[x*2+1], "LINE_POWER_OFF", "Off", ISS_ON);

            //report pulse counts
            IUFillNumber(&lineStatsN[x*4+0], "LINE_COUNTS", "Counts", "%g", 0, 400000000, 1, 0);
            IUFillNumber(&lineStatsN[x*4+1], "LINE_FLUX", "Photon Flux (Lm)", "%g", 0.0, 1.0, 1.0E-5, 0);
            IUFillNumber(&lineStatsN[x*4+2], "LINE_FLUX0", "Flux at mag0 (Lm)", "%g", 0.0, 1.0, 1.0E-5, 0);
            IUFillNumber(&lineStatsN[x*4+3], "LINE_MAGNITUDE", "Estimated magnitude", "%g", -22.0, 22.0, 1.0E-5, 0);

            sprintf(tab, "Line %02d", x+1);
            sprintf(name, "LINE_ENABLE_%02d", x+1);
            IUFillSwitchVector(&lineEnableSP[x], &lineEnableS[x*2], 2, getDeviceName(), name, "Enable Line", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            sprintf(name, "LINE_POWER_%02d", x+1);
            IUFillSwitchVector(&linePowerSP[x], &linePowerS[x*2], 2, getDeviceName(), name, "Power", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            sprintf(name, "LINE_SNOOP_DEVICES_%02d", x+1);
            IUFillTextVector(&lineDevicesTP[x], &lineDevicesT[x*3], 3, getDeviceName(), name, "Locator devices", tab, IP_RW, 60, IPS_IDLE);
            sprintf(name, "LINE_GEOGRAPHIC_COORD_%02d", x+1);
            IUFillNumberVector(&lineGPSNP[x], &lineGPSN[x*3], 3, getDeviceName(), name, "Location", tab, IP_RO, 60, IPS_IDLE);
            sprintf(name, "TELESCOPE_INFO_%02d", x+1);
            IUFillNumberVector(&lineTelescopeNP[x], &lineTelescopeN[x*4], 4, getDeviceName(), name, "Target coordinates", tab, IP_RO, 60, IPS_IDLE);
            sprintf(name, "LINE_DELAY_%02d", x+1);
            IUFillNumberVector(&lineDelayNP[x], &lineDelayN[x], 1, getDeviceName(), name, "Delay line", tab, IP_RO, 60, IPS_IDLE);
            sprintf(name, "LINE_STATS_%02d", x+1);
            IUFillNumberVector(&lineStatsNP[x], &lineStatsN[x*4], 4, getDeviceName(), name, "Stats", tab, IP_RO, 60, IPS_BUSY);
            for (int y = x+1; y < num_lines; y++) {
                sprintf(name, "CORRELATIONS_%0d_%0d", x+1, y+1);
                sprintf(label, "Correlations %d*%d", x+1, y+1);
                IUFillNumber(&correlationsN[idx++], name, label, "%8.0f", 0, 400000000, 1, 0);
                sprintf(name, "COHERENCE_%0d_%0d", x+1, y+1);
                sprintf(label, "Coherence ratio (%d*%d)/(%d+%d)", x+1, y+1, x+1, y+1);
                IUFillNumber(&correlationsN[idx++], name, label, "%1.4f", 0, 1.0, 1, 0);
            }
        }
        IUFillNumberVector(&correlationsNP, correlationsN, ((num_lines*(num_lines-1)/2)*2), getDeviceName(), "CORRELATIONS", "Correlations", "Stats", IP_RO, 60, IPS_BUSY);

        EnableCapture(false);

        SAMPLE_SIZE = sample_size/4;
        NUM_LINES = num_lines;
        DELAY_LINES = delay_lines;

        std::thread(&Interferometer::Callback, this).detach();
        // Start the timer
        SetTimer(POLLMS);
        return true;
    }
    return false;
}

bool Interferometer::SendCommand(it_cmd c, unsigned char value)
{
    return SendChar(c|(value<<4));
}

bool Interferometer::SendChar(char c)
{
    int olen;
    usleep(1000);
    tty_write(PortFD, &c, 1, &olen);
    tcdrain(PortFD);
    return olen == 1;
}

void Interferometer::ActiveLine(int line, bool on, bool power)
{
    SendCommand(SET_ACTIVE_LINE, line);
    SendCommand(SET_LEDS, on | (power << 1));
}

void Interferometer::EnableCapture(bool start)
{
    if(start) {
        SendCommand(ENABLE_CAPTURE, 1);
        gettimeofday(&ExpStart, nullptr);
    } else {
        SendCommand(ENABLE_CAPTURE, 0);
    }
}
