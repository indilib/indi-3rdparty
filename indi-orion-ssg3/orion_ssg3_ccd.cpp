/**
 * INDI driver for Orion Starshoot G3 (and similar) cameras
 *
 * Copyright (c) 2020-2021 Ben Gilsrud
 * Copyright (c) 2021 Jasem Mutlaq
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

#include <iostream>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include "orion_ssg3_ccd.h"
#include "orion_ssg3.h"
#include "config.h"
#include <map>
#include <vector>
#include <unistd.h>

#define MAX_CAMERAS 16
#define TEMP_TIMER_MS 2000

static class Loader
{
        std::map<int, std::shared_ptr<SSG3CCD>> cameras;
        struct orion_ssg3_info camera_infos[MAX_CAMERAS];
    public:
        Loader()
        {
            load();
        }

    public:
        void load()
        {

            int rc;
            int i;
            rc = orion_ssg3_camera_info(camera_infos, MAX_CAMERAS);

            for (i = 0; i < rc; i++)
            {
                SSG3CCD *ssg3Ccd = new SSG3CCD(&camera_infos[i], i);
                cameras[i] = std::shared_ptr<SSG3CCD>(ssg3Ccd);
            }
        }

} loader;

SSG3CCD::SSG3CCD(struct orion_ssg3_info *info, int inst)
{
    InExposure = false;

    ssg3_info = info;
    instance = inst;
    sprintf(name, "%s %d", info->model->name, inst);
    setVersion(ORION_SSG3_VERSION_MAJOR, ORION_SSG3_VERSION_MINOR);
    NSTimer.callOnTimeout(std::bind(&SSG3CCD::stopNSGuide, this));
    NSTimer.setSingleShot(true);
    WETimer.callOnTimeout(std::bind(&SSG3CCD::stopWEGuide, this));
    WETimer.setSingleShot(true);
}

/**
 * Client is asking us to establish connection to the device
 */
bool SSG3CCD::Connect()
{
    uint32_t cap = 0;
    int rc;

    rc = orion_ssg3_open(&ssg3, ssg3_info);
    if (rc)
    {
        LOGF_ERROR("Unable to connect to Orion StartShoot G3: %s\n", strerror(-rc));
        return false;
    }
    LOG_DEBUG("Successfully opened");

    cap |= CCD_CAN_BIN;
    //cap |= CCD_CAN_SUBFRAME;
    cap |= CCD_HAS_COOLER;
    cap |= CCD_HAS_ST4_PORT;
    /* FIXME: kfitsviewer doesn't support CMYG
    if (ssg3.model->color) {
        IUSaveText(&BayerT[0], "0");
        IUSaveText(&BayerT[1], "0");
        IUSaveText(&BayerT[2], "CMYG");
        cap |= CCD_HAS_BAYER;
    } */

    SetCCDCapability(cap);
    TemperatureTimer.callOnTimeout(std::bind(&SSG3CCD::updateTemperature, this));
    TemperatureTimer.start(TEMP_TIMER_MS);

    return true;
}

/**
 * Client is asking us to terminate connection to the device
 */
bool SSG3CCD::Disconnect()
{
    TemperatureTimer.stop();
    saveConfig(true);
    orion_ssg3_close(&ssg3);
    LOG_DEBUG("Successfully disconnected!");
    return true;
}

/**
 * INDI is asking us for our default device name */
const char *SSG3CCD::getDefaultName()
{
    return name;
}

/**
 * INDI is asking us to init our properties.
 */
bool SSG3CCD::initProperties()
{
    /* Must init parent properties first */
    INDI::CCD::initProperties();

    CaptureFormat raw = {"INDI_RAW", "RAW", 16, true};
    addCaptureFormat(raw);

    // Add Debug Control.
    addDebugControl();

    /* Add Gain number property */
    GainNP[0].fill("GAIN", "Gain", "%.f", 0, 255, 1, 185);
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    /* Add Offset number property */
    OffsetNP[0].fill("OFFSET", "Offset", "%.f", 0, 255, 1, 127);
    OffsetNP.fill(getDeviceName(), "CCD_OFFSET", "Offset", IMAGE_SETTINGS_TAB, IP_RW, 0,
                  IPS_IDLE);

    /* Add Cooler power property */
    CoolerPowerNP[0].fill("COOLER_POWER", "Cooler Power (%)", "%.f", 0, 100, 1, 0);
    CoolerPowerNP.fill(getDeviceName(), "CCD_COOLER_POWER", "Cooler Power", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    /* Add Cooler switch */
    CoolerSP[0].fill("COOLER_ON", "On", ISS_OFF);
    CoolerSP[1].fill("COOLER_OFF", "Off", ISS_ON);
    CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_RW,
                  ISR_1OFMANY, 0, IPS_OK);


    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, .001, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 2, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 2, 1, false);

    addAuxControls();

    return true;
}

/*******************************************************************************
** INDI is asking us to update the properties because there is a change in
** CONNECTION status. This fucntion is called whenever the device is
** connected or disconnected.
*******************************************************************************/
bool SSG3CCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        SetTimer(getCurrentPollingPeriod());
        defineProperty(&GainNP);
        defineProperty(&OffsetNP);
        defineProperty(&CoolerSP);
        defineProperty(&CoolerPowerNP);
    }
    else
    {
        deleteProperty(GainNP.getName());
        deleteProperty(OffsetNP.getName());
        deleteProperty(CoolerSP.getName());
        deleteProperty(CoolerPowerNP.getName());
    }

    return true;
}

/*******************************************************************************
** Setting up CCD parameters
*******************************************************************************/
void SSG3CCD::setupParams()
{
    SetCCDParams(orion_ssg3_get_image_width(&ssg3),
                 orion_ssg3_get_image_height(&ssg3),
                 orion_ssg3_get_pixel_bit_size(&ssg3),
                 orion_ssg3_get_pixel_size_x(&ssg3),
                 orion_ssg3_get_pixel_size_y(&ssg3));

    /* calculate how much memory we need for the primary CCD buffer */
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8);
}

/*******************************************************************************
** Set binning
*******************************************************************************/
bool SSG3CCD::UpdateCCDBin(int x, int y)
{
    int rc;

    rc = orion_ssg3_set_binning(&ssg3, x, y);
    if (!rc)
    {
        PrimaryCCD.setBin(x, y);
        return true;
    }
    return false;
}

/**
 * Client is asking to start exposure
 * @param duration: The requested exposure duration, in seconds
 * @return: true if exposure started successfully, false otherwise
 */
bool SSG3CCD::StartExposure(float duration)
{
    int rc;

    ExposureRequest = duration;

    rc = orion_ssg3_start_exposure(&ssg3, duration * 1000);
    if (rc)
    {
        LOGF_ERROR("Failed to start exposure: %d %s\n", rc, strerror(rc));
        return false;
    }
    PrimaryCCD.setExposureDuration(duration);
    ExposureElapsedTimer.start();
    InExposure = true;

    return true;
}

/**
 * Client is asking us to abort an exposure
 */
bool SSG3CCD::AbortExposure()
{
    /* FIXME: Find out if there's a command to send to the SSG3 to abort the exposure */
    InExposure = false;
    return true;
}

/**
 * Client is asking us to set a new number
 */
bool SSG3CCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    int rc;

    if (!strcmp(dev, getDeviceName()))
    {
        if (GainNP.isNameMatch(name))
        {
            if (GainNP.update(values, names, n) == false)
            {
                GainNP.setState(IPS_ALERT);
                GainNP.apply();
                return true;
            }
            LOGF_INFO("Setting gain to %.f", GainNP[0].getValue());
            rc = orion_ssg3_set_gain(&ssg3, GainNP[0].getValue());
            if (!rc)
            {
                GainNP.setState(IPS_OK);
            }
            else
            {
                GainNP.setState(IPS_ALERT);
            }
            GainNP.apply();
            return true;
        }

        if (OffsetNP.isNameMatch(name))
        {
            if (OffsetNP.update(values, names, n) == false)
            {
                OffsetNP.setState(IPS_ALERT);
                OffsetNP.apply();
                return true;
            }
            LOGF_INFO("Setting offset to %.f", OffsetNP[0].getValue());
            rc = orion_ssg3_set_offset(&ssg3, OffsetNP[0].getValue());
            if (!rc)
            {
                OffsetNP.setState(IPS_OK);
            }
            else
            {
                OffsetNP.setState(IPS_ALERT);
            }
            OffsetNP.apply();
            return true;
        }
    }

    // If we didn't process anything above, let the parent handle it.
    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

/**
 * Client is asking us to set a new switch
 */
bool SSG3CCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (CoolerSP.isNameMatch(name))
        {
            if (CoolerSP.update(states, names, n) == false)
            {
                CoolerSP.setState(IPS_ALERT);
                CoolerSP.apply();
                return true;
            }
            IUUpdateSwitch(&CoolerSP, states, names, n);
            CoolerSP.setState(IPS_OK);
            IDSetSwitch(&CoolerSP, nullptr);

            if (CoolerSP[0].getState() == ISS_OFF)
            {
                activateCooler(false);
            }
            else
            {
                activateCooler(true);
            }

            return true;
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

/**
 * Main device loop. We check for exposure progress
 */
void SSG3CCD::TimerHit()
{
    long timeleft;

    if (isConnected() == false)
    {
        return;
    }

    if (InExposure)
    {
        timeleft = ExposureRequest - (ExposureElapsedTimer.elapsed() / 1000);

        if (orion_ssg3_exposure_done(&ssg3))
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            grabImage();
        }
        else
        {
            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    SetTimer(getCurrentPollingPeriod());

    return;
}

/**
 * Save configuration items
 */
bool SSG3CCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &GainNP);
    IUSaveConfigNumber(fp, &OffsetNP);

    return true;
}

/**
 * Download image from SSG3
 */
void SSG3CCD::grabImage()
{
    int rc;
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    int sz = PrimaryCCD.getFrameBufferSize();

    rc = orion_ssg3_image_download(&ssg3, image, sz);
    if (rc)
    {
        LOGF_INFO("Image download failed: %s", strerror(-rc));
    }

    ExposureComplete(&PrimaryCCD);
}

#define TEMP_THRESHOLD 0.25
/**
 * Set the CCD temperature
 */
int SSG3CCD::SetTemperature(double temperature)
{
    LOGF_INFO("Setting temperature to %.2f C.", temperature);

    TemperatureRequest = temperature;

    if (std::abs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    if (!activateCooler(true))
    {
        return -1;
    }

    return 0;
}

bool SSG3CCD::activateCooler(bool enable)
{
    int rc;

    IUResetSwitch(&CoolerSP);
    if (enable)
    {
        rc = orion_ssg3_set_temperature(&ssg3, TemperatureRequest);
        if (rc < 0)
        {
            LOG_ERROR("Failed to turn on cooling.");
            CoolerSP.setState(IPS_ALERT);
            IDSetSwitch(&CoolerSP, nullptr);
            return false;
        }

        CoolerSP[COOLER_ON].setState(ISS_ON);
        CoolerSP[COOLER_OFF].setState(ISS_OFF);
        CoolerSP.setState(IPS_OK);
    }
    else
    {
        rc = orion_ssg3_cooling_off(&ssg3);
        if (rc < 0)
        {
            LOG_ERROR("Failed to turn off cooling.");
            CoolerSP.setState(IPS_ALERT);
            IDSetSwitch(&CoolerSP, nullptr);
            return false;
        }

        CoolerSP[COOLER_ON].setState(ISS_OFF);
        CoolerSP[COOLER_OFF].setState(ISS_ON);
        CoolerSP.setState(IPS_IDLE);
    }

    IDSetSwitch(&CoolerSP, nullptr);
    return true;
}

void SSG3CCD::updateTemperature(void)
{
    float temp;
    int rc;

    rc = orion_ssg3_get_temperature(&ssg3, &temp);
    if (rc < 0)
    {
        TemperatureNP.s = IPS_ALERT;
    }
    else
    {
        LOGF_DEBUG("Read temperature: %f", temp);
        TemperatureN[0].value = temp;
        TemperatureNP.s = IPS_OK;
    }
    IDSetNumber(&TemperatureNP, nullptr);

    if (CoolerSP[COOLER_ON].getState() == ISS_ON)
    {
        rc = orion_ssg3_get_cooling_power(&ssg3, &temp);
        if (rc < 0)
        {
            CoolerPowerNP.setState(IPS_ALERT);
        }
        else
        {
            LOGF_DEBUG("Read temperature: %f", temp);
            CoolerPowerNP[0].setValue(temp);
            CoolerPowerNP.setState(IPS_OK);
        }
        IDSetNumber(&CoolerPowerNP, nullptr);
    }
    else
    {
        CoolerPowerNP[0].setValue(0);
        CoolerPowerNP.setState(IPS_OK);
        IDSetNumber(&CoolerPowerNP, nullptr);
    }
}

void SSG3CCD::stopNSGuide()
{
    NSTimer.stop();
    GuideComplete(AXIS_DE);
}

void SSG3CCD::stopWEGuide()
{
    WETimer.stop();
    GuideComplete(AXIS_RA);
}

IPState SSG3CCD::GuideNorth(uint32_t ms)
{
    if (orion_ssg3_st4(&ssg3, SSG3_GUIDE_NORTH, ms))
    {
        return IPS_ALERT;
    }
    NSTimer.start(ms);
    return IPS_BUSY;
}

IPState SSG3CCD::GuideSouth(uint32_t ms)
{
    if (orion_ssg3_st4(&ssg3, SSG3_GUIDE_SOUTH, ms))
    {
        return IPS_ALERT;
    }
    NSTimer.start(ms);
    return IPS_BUSY;
}

IPState SSG3CCD::GuideEast(uint32_t ms)
{
    if (orion_ssg3_st4(&ssg3, SSG3_GUIDE_EAST, ms))
    {
        return IPS_ALERT;
    }
    WETimer.start(ms);
    return IPS_BUSY;
}

IPState SSG3CCD::GuideWest(uint32_t ms)
{
    if (orion_ssg3_st4(&ssg3, SSG3_GUIDE_WEST, ms))
    {
        return IPS_ALERT;
    }
    WETimer.start(ms);
    return IPS_BUSY;
}
