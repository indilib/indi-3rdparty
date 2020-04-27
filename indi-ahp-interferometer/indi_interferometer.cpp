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
    double *framebuffer = static_cast<double*>(malloc((w*h)*sizeof(double)));
    memset(framebuffer, 0, w*h*sizeof(double));
    char str[MAXINDINAME];
    str[SAMPLE_SIZE] = 0;
    str[HEADER_SIZE] = 0;

    unsigned short cmd = 0x0d3c;
    tcflush(PortFD, TCIOFLUSH);
    tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 2, &olen);

    gettimeofday(&ExpStart, nullptr);

    while (InExposure)
    {
        tty_nread_section(PortFD, buf, FRAME_SIZE, 13, 1, &olen);
        if (olen != FRAME_SIZE)
            continue;
        timeleft -= FRAME_TIME;
        int idx = HEADER_SIZE;
        int center = w*h/2;
        center += w/2;
        unsigned int tmp;
        memset(str, 0, HEADER_SIZE+1);
        sscanf(str, "%08X%08X", &tmp, &power_status);
        for(int x = 0; x < NUM_NODES; x++) {
            IDSetSwitch(&nodeEnableSP[x], nullptr);
            nodeEnableSP[x].sp[0].s = (power_status & (1 << (x + NUM_NODES))) ? ISS_ON : ISS_OFF;
            nodeEnableSP[x].sp[1].s = (power_status & (1 << (x + NUM_NODES))) ? ISS_OFF : ISS_ON;
            IDSetSwitch(&nodePowerSP[x], nullptr);
            nodePowerSP[x].sp[0].s = (power_status & (1 << x)) ? ISS_ON : ISS_OFF;
            nodePowerSP[x].sp[1].s = (power_status & (1 << x)) ? ISS_OFF : ISS_ON;
        }
        for(int x = NUM_NODES-1; x >= 0; x--) {
            memset(str, 0, SAMPLE_SIZE+1);
            strncpy(str, buf+idx, SAMPLE_SIZE);
            sscanf(str, "%X", &tmp);
            counts[x] = tmp;
            totalcounts[x] += counts[x];
            idx += SAMPLE_SIZE;
        }
        for(int x = NUM_BASELINES-1; x >= 0; x--) {
            memset(str, 0, SAMPLE_SIZE+1);
            strncpy(str, buf+idx, SAMPLE_SIZE);
            sscanf(str, "%X", &tmp);
            correlations[x] = tmp;
            totalcorrelations[x] += correlations[x];
            idx += SAMPLE_SIZE;
        }
        idx = 0;
        for(int x = 0; x < NUM_NODES; x++) {
            for(int y = x+1; y < NUM_NODES; y++) {
                INDI::Correlator::UVCoordinate uv = baselines[idx]->getUVCoordinates();
                int xx = static_cast<int>(MAX_RESOLUTION*uv.u/2.0);
                int yy = static_cast<int>(MAX_RESOLUTION*uv.v/2.0);
                int z = center+xx+yy*w;
                if(xx >= -w/2 && xx < w/2 && yy >= -w/2 && yy < h/2) {
                    framebuffer[z] += correlations[idx]*2.0/(counts[x]+counts[y]);
                    framebuffer[w*h-1-z] += correlations[idx]*2.0/(counts[x]+counts[y]);
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
    power_status = 0;

    ExposureRequest = 0.0;
    InExposure = false;

    SAMPLE_SIZE = 0;
    NUM_NODES = 0;
    DELAY_LINES = 0;

    countsN = static_cast<INumber*>(malloc(1));
    countsNP = static_cast<INumberVectorProperty*>(malloc(1));

    nodeEnableS = static_cast<ISwitch*>(malloc(1));
    nodeEnableSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    nodePowerS = static_cast<ISwitch*>(malloc(1));
    nodePowerSP = static_cast<ISwitchVectorProperty*>(malloc(1));

    nodeLocationN = static_cast<INumber*>(malloc(1));
    nodeLocationNP = static_cast<INumberVectorProperty*>(malloc(1));

    correlationsN = static_cast<INumber*>(malloc(1));

    totalcounts = static_cast<double*>(malloc(1));
    totalcorrelations = static_cast<double*>(malloc(1));
    baselines = static_cast<baseline**>(malloc(1));
}

bool Interferometer::Disconnect()
{
    for(int x = 0; x < NUM_NODES*2+1; x++)
        ActiveLine(x, false);
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

    for(int x = 0; x < NUM_NODES; x++) {
        IUSaveConfigNumber(fp, &nodeLocationNP[x]);
        IUSaveConfigSwitch(fp, &nodeEnableSP[x]);
        IUSaveConfigSwitch(fp, &nodePowerSP[x]);
    }
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

    IUFillNumber(&settingsN[0], "INTERFEROMETER_WAVELENGTH_VALUE", "Filter wavelength (m)", "%3.9f", 0.0000003, 999.0, 1.0E-9, 0.211121449);
    IUFillNumberVector(&settingsNP, settingsN, 1, getDeviceName(), "INTERFEROMETER_SETTINGS", "Interferometer Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    uint32_t cap = 0;

    cap |= CCD_CAN_ABORT;
    cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_DSP;

    SetCCDCapability(cap);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 1.0, STELLAR_DAY, 1, false);

    setDefaultPollingPeriod(500);

    tcpConnection = new Connection::TCP(this);
    tcpConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(tcpConnection);

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
        for (int x=0; x<NUM_NODES; x++) {
            defineSwitch(&nodeEnableSP[x]);
        }
        defineNumber(&correlationsNP);
        defineNumber(&settingsNP);

        // Define our properties
    }

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISGetProperties(dev);
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

        for (int x=0; x<NUM_NODES; x++) {
            defineSwitch(&nodeEnableSP[x]);
            if(nodeEnableSP[x].sp[0].s == ISS_ON) {
                defineSwitch(&nodePowerSP[x]);
                defineNumber(&nodeLocationNP[x]);
                defineNumber(&countsNP[x]);
            } else {
                deleteProperty(nodePowerSP[x].name);
                deleteProperty(nodeLocationNP[x].name);
                deleteProperty(countsNP[x].name);
            }
        }
        defineNumber(&correlationsNP);
        defineNumber(&settingsNP);
    }
    else
        // We're disconnected
    {
        deleteProperty(correlationsNP.name);
        deleteProperty(settingsNP.name);
        for (int x=0; x<NUM_NODES; x++) {
            deleteProperty(nodeEnableSP[x].name);
            deleteProperty(nodePowerSP[x].name);
            deleteProperty(nodeLocationNP[x].name);
            deleteProperty(countsNP[x].name);
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
    InExposure = true;
    ExposureRequest = duration;
    timeleft = ExposureRequest;
    PrimaryCCD.setExposureDuration(static_cast<double>(ExposureRequest));
    std::thread(&Interferometer::Callback, this).detach();
    // Start the timer
    SetTimer(POLLMS);

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool Interferometer::AbortExposure()
{
    int olen = 0;
    unsigned short cmd = 0x0d0c;
    tcflush(PortFD, TCIOFLUSH);
    tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 2, &olen);

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

    for (int i = 0; i < NUM_NODES; i++) {
        if(!strcmp(nodeLocationNP[i].name, name)) {
            IUUpdateNumber(&nodeLocationNP[i], values, names, n);
            nodeLocationN[i*3+0].value = values[0];
            nodeLocationN[i*3+1].value = values[1];
            nodeLocationN[i*3+2].value = values[2];
            int idx = 0;
            for(int x = 0; x < NUM_NODES; x++) {
                for(int y = x+1; y < NUM_NODES; y++) {
                    if(x==i||y==i) {

                        INumberVectorProperty *nv = baselines[idx]->getNumber("GEOGRAPHIC_COORD");
                        if(nv != nullptr)
                        {
                            Lat = nv->np[0].value;
                        }
                        Lat *= M_PI/180.0;

                        INDI::Correlator::Baseline b;
                        b.x = (nodeLocationN[x*3+0].value-nodeLocationN[y*3+0].value);
                        b.y = (nodeLocationN[x*3+1].value-nodeLocationN[y*3+1].value)*sin(Lat);
                        b.z = (nodeLocationN[x*3+2].value-nodeLocationN[y*3+2].value)*cos(Lat);
                        b.y += (nodeLocationN[x*3+2].value-nodeLocationN[y*3+2].value)*cos(Lat);
                        b.z += (nodeLocationN[x*3+1].value-nodeLocationN[y*3+1].value)*sin(Lat);
                        baselines[idx]->setBaseline(b);
                    }
                    idx++;
                }
            }
            IDSetNumber(&nodeLocationNP[i], nullptr);
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
    if (strcmp (dev, getDeviceName()))
        return false;

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewSwitch(dev, name, states, names, n);

    for(int x = 0; x < NUM_NODES; x++) {
        if(!strcmp(name, nodeEnableSP[x].name)){
            IUUpdateSwitch(&nodeEnableSP[x], states, names, n);
            updateProperties();
            ActiveLine(x+NUM_NODES, nodeEnableSP[x].sp[0].s == ISS_ON);
            IDSetSwitch(&nodeEnableSP[x], nullptr);
        }
        if(!strcmp(name, nodePowerSP[x].name)){
            IUUpdateSwitch(&nodePowerSP[x], states, names, n);
            updateProperties();
            ActiveLine(x, nodePowerSP[x].sp[0].s == ISS_ON);
            IDSetSwitch(&nodePowerSP[x], nullptr);
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

void Interferometer::ActiveLine(int line, bool on)
{
    int olen = 0;
    unsigned short cmd;
    power_status &= ~(1 << line);
    power_status |= (on << line);
    unsigned long tmp_status = power_status;

    cmd = 0x0d20;
    tcflush(PortFD, TCIOFLUSH);
    tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 2, &olen);
    tcdrain(PortFD);

    unsigned long value = 0x0202020202020202;
    value |= (tmp_status&0xf)<< 4;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 12;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 20;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 28;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 36;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 44;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 52;
    tmp_status >>= 4;
    value |= (tmp_status&0xf)<< 60;
    tmp_status >>= 4;
    tcflush(PortFD, TCIOFLUSH);
    tty_write(PortFD, static_cast<char*>(static_cast<void*>(&value)), 8, &olen);
    cmd = 0xd;
    tcflush(PortFD, TCIOFLUSH);
    tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 1, &olen);
    tcdrain(PortFD);
}

/**************************************************************************************
** Client is asking us to set a new text
***************************************************************************************/
bool Interferometer::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    for(int x = 0; x < NUM_BASELINES; x++)
        baselines[x]->ISNewText(dev, name, texts, names, n);

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
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

    if(InExposure) {
        IDSetNumber(&correlationsNP, nullptr);
        int idx = 0;
        for (int x = 0; x < NUM_NODES; x++) {
            IDSetNumber(&countsNP[x], nullptr);
            countsNP[x].np[0].value = totalcounts[x];
            for(int y = x+1; y < NUM_NODES; y++) {
                correlationsNP.np[idx*2+0].value = totalcorrelations[idx];
                correlationsNP.np[idx*2+1].value = totalcorrelations[idx]/(totalcounts[x]+totalcounts[y]);
                idx++;
            }
        }

        memset(totalcounts, 0, NUM_NODES*sizeof(double));
        memset(totalcorrelations, 0, NUM_BASELINES*sizeof(double));

        // Just update time left in client
        PrimaryCCD.setExposureLeft(static_cast<double>(timeleft));

        SetTimer(POLLMS);
    }
    return;
}

bool Interferometer::Handshake()
{
    PortFD = tcpConnection->getPortFD();
    if(PortFD != -1) {
        int tmp = 0;
        int sample_size = 0;
        int num_nodes = 0;
        int delay_lines = 0;

        int olen;
        char buf[HEADER_SIZE];

        unsigned short cmd;

        cmd = 0x0d3c;
        tcflush(PortFD, TCIOFLUSH);
        tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 2, &olen);

        while (buf[0] != 0xd) {
            int ntries = 10;
            while (ntries-- > 0)
                tty_read(PortFD, buf, 1, 1, &olen);
            if(ntries == 0 || olen != 1) {
                SAMPLE_SIZE = 0;
                NUM_NODES = 0;
                DELAY_LINES = 0;
                return false;
            }
        }

        tty_read(PortFD, buf, HEADER_SIZE, 1, &olen);
        if(olen != HEADER_SIZE) {
            SAMPLE_SIZE = 0;
            NUM_NODES = 0;
            DELAY_LINES = 0;
            return false;
        }

        sscanf(buf, "%02X%02X%02X%02X%08X", &tmp, &sample_size, &num_nodes, &delay_lines, &power_status);

        for(int x = 0; x < NUM_NODES; x++) {
            if(baselines[x] != nullptr) {
                baselines[x]->~baseline();
            }
        }

        countsN = static_cast<INumber*>(realloc(countsN, num_nodes*sizeof(INumber)+1));
        countsNP = static_cast<INumberVectorProperty*>(realloc(countsNP, num_nodes*sizeof(INumberVectorProperty)+1));

        nodeEnableS = static_cast<ISwitch*>(realloc(nodeEnableS, num_nodes*2*sizeof(ISwitch)));
        nodeEnableSP = static_cast<ISwitchVectorProperty*>(realloc(nodeEnableSP, num_nodes*sizeof(ISwitchVectorProperty)+1));

        nodePowerS = static_cast<ISwitch*>(realloc(nodePowerS, num_nodes*2*sizeof(ISwitch)+1));
        nodePowerSP = static_cast<ISwitchVectorProperty*>(realloc(nodePowerSP, num_nodes*sizeof(ISwitchVectorProperty)+1));

        nodeLocationN = static_cast<INumber*>(realloc(nodeLocationN, 3*num_nodes*sizeof(INumber)+1));
        nodeLocationNP = static_cast<INumberVectorProperty*>(realloc(nodeLocationNP, num_nodes*sizeof(INumberVectorProperty)+1));

        correlationsN = static_cast<INumber*>(realloc(correlationsN, 2*(num_nodes*(num_nodes-1)/2)*sizeof(INumber)+1));

        totalcounts = static_cast<double*>(realloc(totalcounts, num_nodes*sizeof(double)+1));
        totalcorrelations = static_cast<double*>(realloc(totalcorrelations, (num_nodes*(num_nodes-1)/2)*sizeof(double)+1));
        baselines = static_cast<baseline**>(realloc(baselines, (num_nodes*(num_nodes-1)/2)*sizeof(baseline*)+1));

        for(int x = 0; x < (num_nodes*(num_nodes-1)/2); x++) {
            baselines[x] = new baseline();
            baselines[x]->initProperties();
        }

        int idx = 0;
        char tab[MAXINDINAME];
        char name[MAXINDINAME];
        char label[MAXINDINAME];
        for (int x = 0; x < num_nodes; x++) {
            IUFillNumber(&nodeLocationN[x*3+0], "NODE_Y", "Latitude offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);
            IUFillNumber(&nodeLocationN[x*3+1], "NODE_X", "Longitude offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);
            IUFillNumber(&nodeLocationN[x*3+2], "NODE_Z", "Elevation offset (m)", "%4.6f", 0.75, 9999.0, .01, 10.0);

            IUFillSwitch(&nodeEnableS[x*2+0], "NODE_ENABLE", "Enable", ISS_OFF);
            IUFillSwitch(&nodeEnableS[x*2+1], "NODE_DISABLE", "Disable", ISS_ON);

            IUFillSwitch(&nodePowerS[x*2+0], "NODE_POWER_ON", "On", ISS_OFF);
            IUFillSwitch(&nodePowerS[x*2+1], "NODE_POWER_OFF", "Off", ISS_ON);

            IUFillNumber(&countsN[x], "NODE_COUNTS", "Counts", "%8.0f", 0, 400000000, 1, 0);

            sprintf(tab, "Node %02d", x+1);
            sprintf(name, "NODE_ENABLE_%02d", x+1);

            IUFillSwitchVector(&nodeEnableSP[x], &nodeEnableS[x*2], 2, getDeviceName(), name, "Enable Node", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

            sprintf(name, "NODE_POWER_%02d", x+1);
            IUFillSwitchVector(&nodePowerSP[x], &nodePowerS[x*2], 2, getDeviceName(), name, "Power", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
            sprintf(name, "NODE_LOCATION_%02d", x+1);
            IUFillNumberVector(&nodeLocationNP[x], &nodeLocationN[x*3], 3, getDeviceName(), name, "Location", tab, IP_RW, 60, IPS_IDLE);
            sprintf(name, "NODE_COUNTS_%02d", x+1);
            IUFillNumberVector(&countsNP[x], &countsN[x], 1, getDeviceName(), name, "Stats", tab, IP_RO, 60, IPS_BUSY);
            for (int y = x+1; y < num_nodes; y++) {
                sprintf(name, "CORRELATIONS_%0d_%0d", x+1, y+1);
                sprintf(label, "Correlations %d*%d", x+1, y+1);
                IUFillNumber(&correlationsN[idx++], name, label, "%8.0f", 0, 400000000, 1, 0);
                sprintf(name, "COHERENCE_%0d_%0d", x+1, y+1);
                sprintf(label, "Coherence ratio (%d*%d)/(%d+%d)", x+1, y+1, x+1, y+1);
                IUFillNumber(&correlationsN[idx++], name, label, "%8.0f", 0, 400000000, 1, 0);
            }
        }
        IUFillNumberVector(&correlationsNP, correlationsN, (num_nodes*(num_nodes-1)/2)*2, getDeviceName(), "CORRELATIONS", "Correlations", "Stats", IP_RO, 60, IPS_BUSY);

        ActiveLine(num_nodes*2, true);

        cmd = 0x0d0c;
        tcflush(PortFD, TCIOFLUSH);
        tty_write(PortFD, static_cast<char*>(static_cast<void*>(&cmd)), 2, &olen);

        SAMPLE_SIZE = sample_size/4;
        NUM_NODES = num_nodes;
        DELAY_LINES = delay_lines;

        return true;
    }
    return false;
}
