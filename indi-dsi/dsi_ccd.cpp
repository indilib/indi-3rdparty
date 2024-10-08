/**
 * Copyright (C) 2015 Ben Gilsrud
 *
 * Modifications and extensions for Meade DSI III Pro by G. Schmidt (gs)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dsi_ccd.h"

#include "config.h"
#include "DsiDeviceFactory.h"

#include <iostream>
#include <math.h>
#include <arpa/inet.h>
#include <unistd.h>

std::unique_ptr<DSICCD> dsiCCD(new DSICCD());

DSICCD::DSICCD()
{
    InExposure = false;
    capturing  = false;
    dsi        = nullptr;

    setVersion(DSI_VERSION_MAJOR, DSI_VERSION_MINOR);
}

/*******************************************************************************
** Client is asking us to establish connection to the device
*******************************************************************************/
bool DSICCD::Connect()
{
    uint32_t cap = 0;
    std::string ccd;

    dsi = DSI::DeviceFactory::getInstance(nullptr);
#ifdef __APPLE__
    if (!dsi)
    {
        sleep(2);
        dsi = DSI::DeviceFactory::getInstance(nullptr);
    }
#endif
    if (!dsi)
    {
        /* The vendor and product ID for all DSI's (I/II/III) are the same.
            * When the Cypress FX2 firmware hasn't been loaded, the PID will
            * be 0x0100. Once the fw is loaded, the PID becomes 0x0101.
            */
        LOG_INFO("Unable to find DSI. Has the firmware been loaded?");
        return false;
    }

    ccd = dsi->getCcdChipName();
    if (ccd == "ICX254AL")
    {
        LOG_INFO("Found a DSI Pro!");
    }
    else if (ccd == "ICX429ALL")
    {
        LOG_INFO("Found a DSI Pro II!");
    }
    else if (ccd == "ICX429AKL")
    {
        LOG_INFO("Found a DSI Color II!");
    }
    else if (ccd == "ICX404AK")
    {
        LOG_INFO("Found a DSI Color!");
    }
    else if (ccd == "ICX285AL")
    {
        LOG_INFO("Found a DSI Pro III!");
    }
    else if (ccd == "ICX285AQ")
    {
        LOG_INFO("Found a DSI Color III!");
    }
    else
    {
        LOGF_INFO("Found a DSI with an unknown CCD: %s", ccd.c_str());
    }

    cap |= CCD_CAN_ABORT;

    if (dsi->isBinnable())
        cap |= CCD_CAN_BIN;

    if (dsi->isColor())
        cap |= CCD_HAS_BAYER;

    SetCCDCapability(cap);

    if (dsi->hasTempSensor())
    {
        CCDTempN[0].value = dsi->ccdTemp();
        IDSetNumber(&CCDTempNP, nullptr);
    }

    CaptureFormat mono = {"INDI_MONO", "Mono", 16, true};
    CaptureFormat color = {"INDI_RAW", "RAW", 16, true};
    addCaptureFormat(dsi->isColor() ? color : mono);

    return true;
}

/*******************************************************************************
** Client is asking us to terminate connection to the device
*******************************************************************************/
bool DSICCD::Disconnect()
{
    delete dsi;
    dsi = nullptr;

    LOG_INFO("Successfully disconnected!");
    return true;
}

/*******************************************************************************
** INDI is asking us for our default device name
*******************************************************************************/

/* changed default name since "/" causes problems when saving properties (gs) */

const char *DSICCD::getDefaultName()
{
    return "DSI";
}

/*******************************************************************************
** INDI is asking us to init our properties.
*******************************************************************************/
bool DSICCD::initProperties()
{
    // Must init parent properties first!
    INDI::CCD::initProperties();

    // Add Debug Control.
    addDebugControl();

    /* Add Gain number property (gs) */
    IUFillNumber(GainN, "GAIN", "Gain", "%g", 0, 100, 1, 100);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    /* Add Offset number property (gs) */
    IUFillNumber(OffsetN, "OFFSET", "Offset", "%g", -50, 50, 1, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", IMAGE_SETTINGS_TAB, IP_RW, 0,
                       IPS_IDLE);

    /* Vdd on during exposure property (gs)
       Actually, Meade envisage leaves Vdd always on for DSI III during
       exposure. However, this results in a significant amount of amp glow.
       Vdd auto mode also seems not to work properly for the DSI III.
       Experimentally, it turned out that swiching Vdd off manually during
       exposure significantly reduces amp glow and also noise by some amount.
       Hence this strategy is used by default but can be changed to
       envisage default mode by setting the following switch to ON.           */
    IUFillSwitch(&VddExpS[0], "Vdd On", "", ISS_OFF);
    IUFillSwitch(&VddExpS[1], "Vdd Off", "", ISS_ON);
    IUFillSwitchVector(&VddExpSP, VddExpS, 2, getDeviceName(), "DSI III exposure", "", IMAGE_SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Add Temp number property (gs) */

    IUFillNumber(CCDTempN, "CCDTEMP", "CCD Temperature [°C]", "%.1f", -128.5, 128.5, 0.1, -128.5);
    IUFillNumberVector(&CCDTempNP, CCDTempN, 1, getDeviceName(), "CCDTemp", "CCD Temp", IMAGE_INFO_TAB, IP_RO, 0,
                       IPS_IDLE);

    /* Set exposure times according to Meade datasheets (gs)
       Ekos probably may limit minimum exposure time to 1/1000 s, but
       setting the lower limit to 1/10000 s might be useful e.g.
       for taking bias frames.                                                */

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.0001, 3600, 1, false);

    setDefaultPollingPeriod(250);

    return true;
}

/*******************************************************************************
** INDI is asking us to update the properties because there is a change in
** CONNECTION status. This fucntion is called whenever the device is
** connected or disconnected.
*******************************************************************************/
bool DSICCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        // Start the timer
        SetTimer(getCurrentPollingPeriod());
        defineProperty(&GainNP);
        defineProperty(&OffsetNP);
        defineProperty(&CCDTempNP);
        defineProperty(&VddExpSP);
    }
    else
    {
        deleteProperty(GainNP.name);
        deleteProperty(OffsetNP.name);
        deleteProperty(CCDTempNP.name);
        deleteProperty(VddExpSP.name);
    }

    return true;
}

/*******************************************************************************
** Setting up CCD parameters
*******************************************************************************/
void DSICCD::setupParams()
{
    SetCCDParams(dsi->getImageWidth(), dsi->getImageHeight(), dsi->getReadBpp() * 8, dsi->getPixelSizeX(),
                 dsi->getPixelSizeY());

    /* calculate how much memory we need for the primary CCD buffer */
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8);

    UpdateCCDBin(PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
}

/*******************************************************************************
** Set binning (currently implemented only for DSI III) (gs)
*******************************************************************************/

bool DSICCD::UpdateCCDBin(int hor, int ver)
{
    if ((hor == 1) && (ver == 1)) // only 1x1 ...
    {
        PrimaryCCD.setBin(hor, ver);
        dsi->set1x1Binning();
        // DSI III 1x1 binning results in a GBRG frame
        if (dsi->getCcdChipName() == "ICX285AQ")
            BayerTP[2].setText("GBRG");
        return true;
    }
    else if ((hor == 2) && (ver == 2)) // ... and 2x2 binning is supported
    {
        PrimaryCCD.setBin(hor, ver);
        dsi->set2x2Binning();
        // DSI III 1x1 binning results in a consolidated mono frame
        if (dsi->getCcdChipName() == "ICX285AQ")
            BayerTP[2].setText("");
        return true;
    }
    else
    {
        IDMessage(getDeviceName(), "Only 1x1 and 2x2 binning is supported by DSI III.");
        LOG_INFO("Only 1x1 and 2x2 binning is supported by DSI III.");
        return false;
    }
}

/*******************************************************************************
** Client is asking us to start an exposure
*******************************************************************************/
bool DSICCD::StartExposure(float duration)
{
    int gain, offset;

    ExposureRequest = duration;

    dsi->setExposureTime(duration);

    /* Since we have only have one CCD with one chip,
       we set the exposure duration of the primary CCD */

    PrimaryCCD.setBPP(dsi->getReadBpp() * 8);
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart, nullptr);

    InExposure = true;
    LOG_INFO("Exposure has begun.");

    /* Adjust gain and offset (gs)
       The gain is normalized in the same way as in Meade envisage (0..100)
       while the offset takes the values (-50..50) instead of (0..10) to
       reflect that positive and negative offsets may be set                  */

    gain   = (int)round(GainN[0].value / 100.0 * 63);   // normalize 100% -> 63
    offset = (int)round(OffsetN[0].value / 50.0 * 255); // normalize 50% -> 255

    /* negative offset values */
    offset = (offset >= 0 ? offset : 256 - offset);

    dsi->startExposure(duration * 10000, gain, offset);

    return true;
}

/*******************************************************************************
** Client is asking us to abort an exposure
*******************************************************************************/
bool DSICCD::AbortExposure()
{
    InExposure = false;
    return true;
}

/*******************************************************************************
** How much longer until exposure is done?
*******************************************************************************/
float DSICCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

/*******************************************************************************
** Client is asking us to set a new number
*******************************************************************************/
bool DSICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, GainNP.name))
        {
            IUUpdateNumber(&GainNP, values, names, n);
            GainNP.s = IPS_OK;
            IDSetNumber(&GainNP, nullptr);

            return true;
        }

        if (!strcmp(name, OffsetNP.name))
        {
            IUUpdateNumber(&OffsetNP, values, names, n);
            OffsetNP.s = IPS_OK;
            IDSetNumber(&OffsetNP, nullptr);

            return true;
        }
    }

    // If we didn't process anything above, let the parent handle it.
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/*******************************************************************************
** Client is asking us to set a new switch
*******************************************************************************/

bool DSICCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (strcmp(dev, getDeviceName()) == 0)
    {
        /* Vdd on/off switch for DSI III exposure control (gs) */
        if (!strcmp(name, VddExpSP.name))
        {
            if (IUUpdateSwitch(&VddExpSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&VddExpSP);

            if (index == 0)
                dsi->setVddOn(true);
            else
                dsi->setVddOn(false);

            VddExpSP.s = IPS_OK;
            IDSetSwitch(&VddExpSP, index == 0 ? "Vdd mode is ON" : "Vdd mode is OFF");

            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/*******************************************************************************
** Main device loop. We check for exposure progress
*******************************************************************************/
void DSICCD::TimerHit()
{
    long timeleft;

    if (isConnected() == false)
    {
        return; //  No need to reset timer if we are not connected anymore
    }

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        /* Exposure control has been changed to ensure stable operation
           for short exposures as well as for long exposures (gs)             */

        if (!dsi->ExposureInProgress())
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            InExposure = false;

            /* grab and save image */
            grabImage();

            /* update temperature as measured after exposure (gs)
               Attention: no continous temperature update, we always diplay
               temperature for last exposure (interesting for matching darks) */

            if (dsi->hasTempSensor())
            {
                CCDTempN[0].value = dsi->ccdTemp();
                IDSetNumber(&CCDTempNP, nullptr);
            }
        }
        else
        {
            // Just update time left in client
            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}

/*******************************************************************************
 * Save configuration items (gs)
*******************************************************************************/

bool DSICCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &GainNP);
    IUSaveConfigNumber(fp, &OffsetNP);
    IUSaveConfigSwitch(fp, &VddExpSP);

    return true;
}

/*******************************************************************************
 * Download image from DSI
*******************************************************************************/

void DSICCD::grabImage()
{
    uint16_t *buf = nullptr;
    int x = 0, y = 0;

    std::unique_lock<std::mutex> guard(ccdBufferLock);
    // Let's get a pointer to the frame buffer
    uint8_t *image = PrimaryCCD.getFrameBuffer();

    // Get width and height
    int width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    try
    {
        buf = (uint16_t *)dsi->ccdFramebuffer();
    }
    catch (...)
    {
        LOG_INFO("Image download failed!");
        return;
    }
    guard.unlock();

    for (y = 0; y < height; ++y)
    {
        for (x = 0; x < width; ++x)
        {
            ((uint16_t *)image)[x + width * y] = ntohs(buf[x + width * y]);
        }
    }

    delete buf;

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);

    LOG_INFO("Exposure complete.");
}

/******************************************************************************/
