/*
    indi_LMS_spectrograph - a software defined radio driver for INDI
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

#include "indi_limesdr_spectrograph.h"
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
static LIMESDR *receivers[MAX_DEVICES];
static lms_info_str_t *lime_dev_list;

static void cleanup()
{
    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        delete receivers[i];
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iNumofConnectedSpectrographs = 0;

        iNumofConnectedSpectrographs = LMS_GetDeviceList(lime_dev_list);
        if (iNumofConnectedSpectrographs > MAX_DEVICES)
            iNumofConnectedSpectrographs = MAX_DEVICES;
        if (iNumofConnectedSpectrographs <= 0)
        {
            //Try sending IDMessage as well?
            IDLog("No LIMESDR receivers detected. Power on?");
            IDMessage(nullptr, "No LIMESDR receivers detected. Power on?");
        }
        else
        {
            for (int i = 0; i < iNumofConnectedSpectrographs; i++)
            {
                receivers[i] = new LIMESDR(i);
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
        IDMessage(nullptr, "No LIMESDR receivers detected. Power on?");
        return;
    }

    for (int i = 0; i < iNumofConnectedSpectrographs; i++)
    {
        LIMESDR *receiver = receivers[i];
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
        LIMESDR *receiver = receivers[i];
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
        LIMESDR *receiver = receivers[i];
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
        LIMESDR *receiver = receivers[i];
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
        LIMESDR *receiver = receivers[i];
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
        LIMESDR *receiver = receivers[i];
        receiver->ISSnoopDevice(root);
    }
}

LIMESDR::LIMESDR(uint32_t index)
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
bool LIMESDR::Connect()
{
    int r = LMS_Open(&lime_dev, lime_dev_list[spectrographIndex], NULL);
    if (r < 0)
    {
        LOGF_ERROR("Failed to open limesdr device index %d.", spectrographIndex);
        return false;
    }
    LMS_Init(lime_dev);
    LMS_EnableChannel(lime_dev, LMS_CH_RX, 0, true);
    LOG_INFO("LIME-SDR Spectrograph connected successfully!");
    // Let's set a timer that checks teleSpectrographs status every POLLMS milliseconds.
    // JM 2017-07-31 SetTimer already called in updateProperties(). Just call it once
    //SetTimer(POLLMS);

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool LIMESDR::Disconnect()
{
    InIntegration = false;
    LMS_Close(lime_dev);
    setBufferSize(1);
    LOG_INFO("LIME-SDR Spectrograph disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *LIMESDR::getDefaultName()
{
    return "LIME-SDR Receiver";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool LIMESDR::initProperties()
{
    // We set the Spectrograph capabilities
    uint32_t cap = SENSOR_CAN_ABORT | SENSOR_HAS_STREAMING | SENSOR_HAS_DSP;
    SetSpectrographCapability(cap);

    // Must init parent properties first!
    INDI::Spectrograph::initProperties();

    setMinMaxStep("SENSOR_INTEGRATION", "SENSOR_INTEGRATION_VALUE", 0.001, 86164.092, 0.001, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_FREQUENCY", 400.0e+6, 3.8e+9, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_SAMPLERATE", 2.0e+6, 28.0e+6, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_GAIN", 0.0, 1.0, 0.01, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BANDWIDTH", 400.0e+6, 3.8e+9, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BITSPERSAMPLE", -32, -32, 0, false);
    setIntegrationFileExtension("fits");
    /*
    // PrimarySpectrograph Device Continuum Blob
    IUFillBLOB(&TFitsB[0], "TRMT", "Transmit1", "");
    IUFillBLOB(&TFitsB[1], "TRMT", "Transmit2", "");
    IUFillBLOB(&TFitsB[2], "TRMT", "Transmit3", "");
    IUFillBLOB(&TFitsB[3], "TRMT", "Transmit4", "");
    IUFillBLOB(&TFitsB[4], "TRMT", "Transmit5", "");
    IUFillBLOBVector(&TFitsBP, TFitsB, 5, getDeviceName(), "LIME_TRMT", "Transmit Data", INTEGRATION_INFO_TAB, IP_WO, 60, IPS_IDLE);
*/
    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool LIMESDR::updateProperties()
{
    // Call parent update properties first
    INDI::Spectrograph::updateProperties();

    if (isConnected())
    {
        // Inital values
        setupParams(1000000, 1420000000, 10000, 10);
        //defineBLOB(&TFitsBP);

        // Start the timer
        SetTimer(POLLMS);
    }
    else
    {
        //deleteProperty(TFitsBP.name);
    }

    return true;
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool LIMESDR::StartIntegration(float duration)
{
    IntegrationRequest = duration;

    // Since we have only have one Spectrograph with one chip, we set the exposure duration of the primary Spectrograph
    setIntegrationTime(duration);
    b_read  = 0;
    to_read = getSampleRate() * getIntegrationTime();

    setBufferSize(to_read * sizeof(float));

    if (to_read > 0)
    {
        lime_stream.channel             = 0;
        lime_stream.isTx                = false;
        lime_stream.fifoSize            = to_read;
        lime_stream.dataFmt             = lms_stream_t::LMS_FMT_F32;
        lime_stream.throughputVsLatency = 0.5;
        LMS_SetupStream(lime_dev, &lime_stream);
        LMS_StartStream(&lime_stream);
        gettimeofday(&CapStart, nullptr);
        InIntegration = true;
        LOG_INFO("Integration started...");
        return true;
    }

    // We're done
    return false;
}

/**************************************************************************************
** Client is updating capture settings
***************************************************************************************/
void LIMESDR::setupParams(float sr, float freq, float bw, float gain)
{
    setBPS(-32);
    int r = 0;
    r |= LMS_SetAntenna(lime_dev, LMS_CH_RX, 0, 0);
    r |= LMS_SetNormalizedGain(lime_dev, LMS_CH_RX, 0, gain);
    r |= LMS_SetLOFrequency(lime_dev, LMS_CH_RX, 0, freq);
    r |= LMS_SetSampleRate(lime_dev, sr, 0);
    r |= LMS_Calibrate(lime_dev, LMS_CH_RX, 0, bw, 0);

    if (r != 0)
    {
        LOG_INFO("Error(s) setting parameters.");
    }
}

bool LIMESDR::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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
** Client is asking us to abort a capture
***************************************************************************************/
bool LIMESDR::AbortIntegration()
{
    if (InIntegration)
    {
        lms_stream_status_t status;
        LMS_GetStreamStatus(&lime_stream, &status);
        if (status.fifoFilledCount <= 0)
        {
            InIntegration = false;
            LMS_StopStream(&lime_stream);
            LMS_DestroyStream(lime_dev, &lime_stream);
        }
    }
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float LIMESDR::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(CapStart.tv_sec * 1000.0 + CapStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = IntegrationRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for capture progress here
***************************************************************************************/
void LIMESDR::TimerHit()
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
            lms_stream_status_t status;
            LMS_GetStreamStatus(&lime_stream, &status);
            if (status.active)
            {
                if (status.fifoFilledCount >= status.fifoSize)
                {
                    n_read = status.fifoFilledCount;
                    grabData();
                }
            }
            timeleft = 0.0;
        }

        // This is an over simplified timing method, check SpectrographSimulator and limesdrSpectrograph for better timing checks
        setIntegrationLeft(timeleft);
    }

    SetTimer(POLLMS);
    return;
}

/**************************************************************************************
** Create the spectrum
***************************************************************************************/
void LIMESDR::grabData()
{
    if (InIntegration)
    {
        continuum = getBuffer();
        LOG_INFO("Downloading...");
        LMS_RecvStream(&lime_stream, continuum, n_read, NULL, 1000);
        LMS_StopStream(&lime_stream);
        LMS_DestroyStream(lime_dev, &lime_stream);
        InIntegration = false;

        LOG_INFO("Download complete.");
        IntegrationComplete();
    }
}
