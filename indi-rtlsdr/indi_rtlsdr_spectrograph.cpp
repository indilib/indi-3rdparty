/*
    indi_rtlsdr_spectrograph - a software defined radio driver for INDI
    Copyright (C) 2017  Ilia Platone

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

#include "indi_rtlsdr_spectrograph.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <indilogger.h>
#include <memory>

#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })
#define MAX_TRIES      20
#define MAX_DEVICES    4
#define SUBFRAME_SIZE  (16384)
#define MIN_FRAME_SIZE (512)
#define MAX_FRAME_SIZE (SUBFRAME_SIZE * 16)
#define SPECTRUM_SIZE  (256)

static int iNumofConnectedSpectrographs;
static RTLSDR *receivers[MAX_DEVICES];

static void cleanup()
{
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        delete receivers[i];
    }
}

void RTLSDR::Callback()
{
    int len            = min(MAX_FRAME_SIZE, to_read);
    int olen           = 0;
    unsigned char *buf = (unsigned char *)malloc(len);
    rtlsdr_reset_buffer(rtl_dev);
    while (InIntegration)
    {
        rtlsdr_read_sync(rtl_dev, buf, len, &olen);
        buffer = buf;
        n_read = olen;
        grabData();
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iNumofConnectedSpectrographs = 0;

        iNumofConnectedSpectrographs = rtlsdr_get_device_count();
        if (iNumofConnectedSpectrographs > MAX_DEVICES)
            iNumofConnectedSpectrographs = MAX_DEVICES;
        if (iNumofConnectedSpectrographs <= 0)
        {
            //Try sending IDMessage as well?
            IDLog("No RTLSDR receivers detected. Power on?");
            IDMessage(nullptr, "No RTLSDR receivers detected. Power on?");
        }
        else
        {
            for (int i = 0; i < iNumofConnectedSpectrographs; i++)
            {
                receivers[i] = new RTLSDR(i);
            }
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();

    if (iNumofConnectedSpectrographs == 0)
    {
        IDMessage(nullptr, "No RTLSDR receivers detected. Power on?");
        return;
    }

    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
        {
            receiver->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
        {
            receiver->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
        {
            receiver->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
        {
            receiver->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int num)
{
    ISInit();
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
        {
            receiver->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        RTLSDR *receiver = receivers[i];
        receiver->ISSnoopDevice(root);
    }
}

RTLSDR::RTLSDR(uint32_t index)
{
    InIntegration = false;
    spectrographIndex = index;

    char name[MAXINDIDEVICE];
    snprintf(name, MAXINDIDEVICE, "%s %d", getDefaultName(), index);
    setDeviceName(name);
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RTLSDR::Connect()
{
    int r = rtlsdr_open(&rtl_dev, spectrographIndex);
    if (r < 0)
    {
        LOGF_ERROR("Failed to open rtlsdr device index %d.", spectrographIndex);
        return false;
    }

    LOG_INFO("RTL-SDR Spectrograph connected successfully!");
    // Let's set a timer that checks teleSpectrographs status every POLLMS milliseconds.
    // JM 2017-07-31 SetTimer already called in updateProperties(). Just call it once
    //SetTimer(POLLMS);

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RTLSDR::Disconnect()
{
    InIntegration = false;
    rtlsdr_close(rtl_dev);
    setBufferSize(1);
    LOG_INFO("RTL-SDR Spectrograph disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RTLSDR::getDefaultName()
{
    return "RTL-SDR Receiver";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RTLSDR::initProperties()
{
    // We set the Spectrograph capabilities
    uint32_t cap = SENSOR_CAN_ABORT | SENSOR_HAS_STREAMING | SENSOR_HAS_DSP;
    SetSpectrographCapability(cap);

    // Must init parent properties first!
    INDI::Spectrograph::initProperties();

    setMinMaxStep("SPECTROGRAPH_INTEGRATION", "SPECTROGRAPH_INTEGRATION_VALUE", 0.001, 86164.092, 0.001, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_FREQUENCY", 2.4e+7, 2.0e+9, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_SAMPLERATE", 1.0e+6, 2.0e+6, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_GAIN", 0.0, 25.0, 0.1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BANDWIDTH", 0, 0, 0, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BITSPERSAMPLE", 16, 16, 0, false);
    setIntegrationFileExtension("fits");

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool RTLSDR::updateProperties()
{
    // Call parent update properties first
    INDI::Spectrograph::updateProperties();

    if (isConnected())
    {
        // Inital values
        setupParams(1000000, 1420000000, 10000, 10);

        // Start the timer
        SetTimer(POLLMS);
    }

    return true;
}

/**************************************************************************************
** Setting up Spectrograph parameters
***************************************************************************************/
void RTLSDR::setupParams(float sr, float freq, float bw, float gain)
{
    setBandwidth(bw);
    setFrequency(freq);
    setGain(gain);
    setSampleRate(sr);
    setBPS(16);
    int r = 0;

    r |= rtlsdr_set_agc_mode(rtl_dev, 0);
    r |= rtlsdr_set_tuner_gain_mode(rtl_dev, 1);
    r |= rtlsdr_set_tuner_gain(rtl_dev, (int)(gain * 10));
    r |= rtlsdr_set_tuner_bandwidth(rtl_dev, (uint32_t)bw);
    r |= rtlsdr_set_center_freq(rtl_dev, (uint32_t)freq);
    r |= rtlsdr_set_sample_rate(rtl_dev, (uint32_t)sr);

    if (r != 0)
    {
        LOG_INFO("Error(s) setting parameters.");
    }
}

bool RTLSDR::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool r = false;
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, SpectrographSettingsNP.name)) {
        for(int i = 0; i < n; i++) {
            if (!strcmp(names[i], "SPECTROGRAPH_GAIN")) {
                setupParams(getSampleRate(), getFrequency(), getBandwidth(), values[i]);
            } else if (!strcmp(names[i], "SPECTROGRAPH_BANDWIDTH")) {
                setupParams(getSampleRate(), getFrequency(), values[i], getGain());
            } else if (!strcmp(names[i], "SPECTROGRAPH_FREQUENCY")) {
                setupParams(getSampleRate(), values[i], getBandwidth(), getGain());
            } else if (!strcmp(names[i], "SPECTROGRAPH_SAMPLERATE")) {
                setupParams(values[i], getFrequency(), getBandwidth(), getGain());
            }
        }
        IDSetNumber(&SpectrographSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n) & !r;
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RTLSDR::StartIntegration(double duration)
{
    IntegrationRequest = duration;
    AbortIntegration();

    // Since we have only have one Spectrograph with one chip, we set the exposure duration of the primary Spectrograph
    setIntegrationTime(duration);
    b_read  = 0;
    to_read = getSampleRate() * getIntegrationTime() * sizeof(unsigned short);

    setBufferSize(to_read);

    if (to_read > 0)
    {
        LOG_INFO("Integration started...");
        // Run threads
        std::thread(&RTLSDR::Callback, this).detach();
        gettimeofday(&IntStart, nullptr);
        InIntegration = true;
        return true;
    }

    // We're done
    return false;
}

/**************************************************************************************
** Client is asking us to abort a capture
***************************************************************************************/
bool RTLSDR::AbortIntegration()
{
    if (InIntegration)
    {
        InIntegration = false;
    }
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float RTLSDR::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(IntStart.tv_sec * 1000.0 + IntStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = IntegrationRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for capture progress here
***************************************************************************************/
void RTLSDR::TimerHit()
{
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InIntegration)
    {
        timeleft = CalcTimeLeft();
        if (timeleft < 0.1)
        {
            /* We're done capturing */
            LOG_INFO("Integration done, expecting data...");
            timeleft = 0.0;
        }

        // This is an over simplified timing method, check SpectrographSimulator and rtlsdrSpectrograph for better timing checks
        setIntegrationLeft(timeleft);
    }

    SetTimer(POLLMS);
    return;
}

/**************************************************************************************
** Create the spectrum
***************************************************************************************/
void RTLSDR::grabData()
{
    if (InIntegration)
    {
        n_read    = min(to_read, n_read);
        continuum = getBuffer();
        if (n_read > 0)
        {
            memcpy(continuum + b_read, buffer, n_read);
            b_read += n_read;
            to_read -= n_read;
        }

        if (to_read <= 0)
        {
            InIntegration = false;
            LOG_INFO("Download complete.");
            IntegrationComplete();
        }
    }
}
