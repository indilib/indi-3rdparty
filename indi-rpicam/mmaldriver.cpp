/*
 Raspberry Pi High Quality Camera CCD Driver for Indi.
 Copyright (C) 2020 Lars Berntzon (lars.berntzon@cecilia-data.se).
 All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * INDI driver for Raspberry Pi 8Mp and 12Mp High Quality camera.
 */
#include <algorithm>
#include <memory>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <bcm_host.h>

#include "mmalexception.h"
#include "mmaldriver.h"
#include "cameracontrol.h"
#include "jpegpipeline.h"
#include "broadcompipeline.h"
#include "raw10tobayer16pipeline.h"
#include "raw12tobayer16pipeline.h"
#include "pipetee.h"
#include "inditest.h"

VCOS_LOG_CAT_T indi_rpicam_log_category;

MMALDriver::MMALDriver() : INDI::CCD(), chipWrapper(&PrimaryCCD)
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    setVersion(INDI_RPICAM_VERSION_MAJOR, INDI_RPICAM_VERSION_MINOR);

    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("indi_rpicam", &indi_rpicam_log_category);

    LOGF_DEBUG("%s() - returning", __FUNCTION__);
}

MMALDriver::~MMALDriver()
{
    LOG_DEBUG("MMALDriver::~MMALDriver()");
}

void MMALDriver::assert_framebuffer(INDI::CCDChip *ccd)
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    int nbuf = (ccd->getXRes() * ccd->getYRes() * (ccd->getBPP() / 8));
    int expected = 4056 * 3040 * 2;
    if (nbuf != expected)
    {
        LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);
        LOGF_ERROR("%s: Wrong size of framebuffer: %d, expected %d", __FUNCTION__, nbuf, expected);
        exit(1);
    }

    LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);
}

bool MMALDriver::saveConfigItems(FILE * fp)
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    INDI::CCD::saveConfigItems(fp);

#ifdef USE_ISO
    // ISO Settings
    if (mIsoSP.nsp > 0)
    {
        IUSaveConfigSwitch(fp, &mIsoSP);
    }
#endif

    // Gain Settings
    IUSaveConfigNumber(fp, &mGainNP);

    return true;
}

void MMALDriver::addFITSKeywords(INDI::CCDChip * targetChip)
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    INDI::CCD::addFITSKeywords(targetChip);

#ifdef USE_ISO
    int status = 0;
    if (mIsoSP.nsp > 0)
    {
        ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
        if (onISO)
        {
            int isoSpeed = atoi(onISO->label);
            if (isoSpeed > 0)
            {
                fits_update_key_s(*targetChip->fitsFilePointer(), TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
            }
        }
    }
#endif

}

/**************************************************************************************
 * @brief capture_complete Called by the MMAL callback routine (other thread) when whole capture is done.
 **************************************************************************************/
void MMALDriver::capture_complete()
{
    LOGF_DEBUG("%s", __FUNCTION__);
    exposure_thread_done = true;
}

/**************************************************************************************
 * Client is asking us to establish connection to the device
 **************************************************************************************/
bool MMALDriver::Connect()
{
    LOGF_DEBUG("%s()", __FUNCTION__);

    SetTimer(getCurrentPollingPeriod());

    camera_control.reset(new CameraControl());

    camera_control->add_capture_listener(this);

    setupPipeline();

    camera_control->add_pipeline(raw_pipe.get());


    SetCCDParams(static_cast<int>(camera_control->get_camera()->get_width()),
                 static_cast<int>(camera_control->get_camera()->get_height()),
                 16,
                 camera_control->get_camera()->xPixelSize,
                 camera_control->get_camera()->yPixelSize);

    uint32_t nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);
    //V1 cam
    if (!strcmp(camera_control->get_camera()->getModel(), "ov5647"))
    {
        IUSaveText(&BayerT[2], "GBRG");
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 6, .0001, false);
    }
    //V2 cam
    else if (!strcmp(camera_control->get_camera()->getModel(), "imx219"))
    {
        IUSaveText(&BayerT[2], "BGGR");
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 10, .0001, false);
    }
    //HQ cam
    else if (!strcmp(camera_control->get_camera()->getModel(), "imx477"))
    {
        IUSaveText(&BayerT[2], "BGGR");
        PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 200, .0001, false);
    }

    return true;
}

/**************************************************************************************
 * Client is asking us to terminate connection to the device
 **************************************************************************************/
bool MMALDriver::Disconnect()
{
    LOGF_DEBUG("%s()", __FUNCTION__);

    camera_control.reset();

    return true;
}

/**************************************************************************************
 * INDI is asking us for our default device name
 **************************************************************************************/
const char *MMALDriver::getDefaultName()
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    return "RPI Camera";
}

void MMALDriver::ISGetProperties(const char * dev)
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    if (dev != nullptr && strcmp(getDeviceName(), dev) != 0)
    {
        return;
    }

    INDI::CCD::ISGetProperties(dev);
}

bool MMALDriver::initProperties()
{
    LOGF_DEBUG("%s()", __FUNCTION__);

    // We must ALWAYS init the properties of the parent class first
    INDI::CCD::initProperties();

    // ISO switches
#ifdef USE_ISO
    IUFillSwitch(&mIsoS[0], "ISO_100", "100", ISS_OFF);
    IUFillSwitch(&mIsoS[1], "ISO_200", "200", ISS_OFF);
    IUFillSwitch(&mIsoS[2], "ISO_400", "400", ISS_ON);
    IUFillSwitch(&mIsoS[3], "ISO_800", "800", ISS_OFF);
    IUFillSwitchVector(&mIsoSP, mIsoS, 4, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);
#endif

    // CCD Gain
    IUFillNumber(&mGainN[0], "GAIN", "Gain", "%.f", 1, 16.0, 1, 1);
    IUFillNumberVector(&mGainNP, mGainN, 1, getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    addDebugControl();

    SetCCDCapability(0
                     | CCD_CAN_BIN			// Does the CCD support binning?
                     | CCD_CAN_SUBFRAME		// Does the CCD support setting ROI?
                     //	| CCD_CAN_ABORT			// Can the CCD exposure be aborted?
                     //	| CCD_HAS_GUIDE_HEAD	// Does the CCD have a guide head?
                     //	| CCD_HAS_ST4_PORT 		// Does the CCD have an ST4 port?
                     //	| CCD_HAS_SHUTTER 		// Does the CCD have a mechanical shutter?
                     //	| CCD_HAS_COOLER 		// Does the CCD have a cooler and temperature control?
                     | CCD_HAS_BAYER 		// Does the CCD send color data in bayer format?
                     //	| CCD_HAS_STREAMING 	// Does the CCD support live video streaming?
                     //	| CCD_HAS_WEB_SOCKET 	// Does the CCD support web socket transfers?
                    );


    setDefaultPollingPeriod(500);

    CaptureFormat format = {"INDI_RAW", "RAW 16", 16, true};
    addCaptureFormat(format);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 1000, .0001, false);

    return true;
}

bool MMALDriver::updateProperties()
{
    LOGF_DEBUG("%s()", __FUNCTION__);
    // We must ALWAYS call the parent class updateProperties() first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (!strcmp(camera_control->get_camera()->getModel(), "ov5647"))
        {
            IUSaveText(&BayerT[2], "GBRG");
        }
        else
        {
            IUSaveText(&BayerT[2], "BGGR");
        }

#ifdef USE_ISO
        if (mIsoSP.nsp > 0)
        {
            defineProperty(&mIsoSP);
        }
#endif
        defineProperty(&mGainNP);
    }
    else
    {
#ifdef USE_ISO
        if (mIsoSP.nsp > 0)
        {
            deleteProperty(mIsoSP.name);
        }
#endif

        deleteProperty(mGainNP.name);
    }

    return true;
}

// FIXME: implement UpdateCCDBin
bool MMALDriver::UpdateCCDBin(int hor, int ver)
{
    LOGF_DEBUG("%s(%d, %d)", __FUNCTION__, hor, ver);

    return true;
}

/**
 * CCD calls this function when CCD Frame dimension needs to be updated in the hardware.
 *
 * Derived classes should implement this function.
 * @param x start x pos
 * @param y start y pos
 * @param w width
 * @param h height
 */
bool MMALDriver::UpdateCCDFrame(int x, int y, int w, int h)
{
    LOGF_DEBUG("%s(%d, %d, %d, %d)", __FUNCTION__, x, y, w, h);

    PrimaryCCD.setFrame(x, y, w, h);

    // Total bytes required for image buffer
    uint32_t nbuf = (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * PrimaryCCD.getBPP() / 8);

    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

/**
 * Client is asking us to start an exposure
 *
 * @param duration exposure time in seconds.
 */
bool MMALDriver::StartExposure(float duration)
{
    LOGF_DEBUG("%s(%f)", __FUNCTION__, duration);
    assert(PrimaryCCD.getFrameBuffer() != 0);


    if (InExposure)
    {
        LOG_ERROR("Camera is already exposing.");
        return false;
    }

    exposure_thread_done = false;

    ExposureRequest = static_cast<double>(duration);

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));

    gettimeofday(&ExpStart, nullptr);

    InExposure = true;

#ifdef USE_ISO
    int isoSpeed = 0;

    isoSpeed = DEFAULT_ISO;
    ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
    if (onISO)
    {
        isoSpeed = atoi(onISO->label);
    }
#endif
    double gain = mGainN[0].value;


    ccdBufferLock.lock();

    raw_pipe->reset_pipe();

    image_buffer_pointer = PrimaryCCD.getFrameBuffer();
    try
    {
#ifdef USE_ISO
        camera_control->get_camera()->setISO(isoSpeed);
#endif
        camera_control->setGain(gain);
        camera_control->setShutterSpeed(static_cast<long>(ExposureTime * 1000000));
        camera_control->get_camera()->set_crop(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(),
                                               PrimaryCCD.getSubH());
        camera_control->startCapture();
    }
    catch (MMALException &e)
    {
        LOGF_ERROR("%s(%s): Caugh camera exception: %s\n", __FILE__, __func__, e.what());
        return false;
    }

    IDMessage(getDeviceName(), "Download complete.");

    // Return true for this will take some time.
    return true;
}


/**************************************************************************************
 * Client is asking us to abort an exposure
 **************************************************************************************/
bool MMALDriver::AbortExposure()
{
    LOGF_DEBUG("%s()", __FUNCTION__);

    camera_control->stopCapture();
    ccdBufferLock.unlock();
    InExposure = false;
    PrimaryCCD.setExposureLeft(0);

    camera_control.reset();

    return true;
}

/**************************************************************************************
 * How much longer until exposure is done?
 **************************************************************************************/
double MMALDriver::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince = static_cast<double>(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                static_cast<double>(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;

    if (timeleft < 0)
    {
        timeleft = 0;
    }

    return timeleft;
}

/**************************************************************************************
 * Main device loop. We check for exposure
 **************************************************************************************/
void MMALDriver::TimerHit()
{
    uint32_t nextTimer = getCurrentPollingPeriod();

    if (!isConnected())
    {
        return; //  No need to reset timer if we are not connected anymore
    }

    if (InExposure)
    {
        double timeleft = CalcTimeLeft();
        if (timeleft < 0) timeleft = 0;

        // Just update time left in client
        PrimaryCCD.setExposureLeft(timeleft);

        // Less than a 1 second away from exposure completion, use shorter timer. If less than 1m, take the image.
        if (exposure_thread_done)
        {
            /* We're done exposing */
            IDMessage(getDeviceName(), "Exposure done, downloading image...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            ccdBufferLock.unlock();
            InExposure = false;

            // Stop capturing (must be done from main thread).
            camera_control->stopCapture();

            // Let INDI::CCD know we're done filling the image buffer
            LOG_DEBUG("Exposure complete.");
            ExposureComplete(&PrimaryCCD);
        }
    }

    SetTimer(nextTimer);
}

bool MMALDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    LOGF_DEBUG("%s(%s, %s,\n", __FUNCTION__, dev, name);
    for(int i = 0; i < n; i++)
    {
        LOGF_DEBUG("      value:%d, name: %s,\n", states[i], names[i]);
    }
    LOG_DEBUG(")\n");

    // ignore if not ours
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return false;

    if (INDI::CCD::ISNewSwitch(dev, name, states, names, n))
        return true;

    ISwitchVectorProperty *svp = getSwitch(name);
    if (!isConnected())
    {
        svp->s = IPS_ALERT;
        IDSetSwitch(svp, "Cannot change property while device is disconnected.");
        return false;
    }

#ifdef USE_ISO
    if (!strcmp(name, mIsoSP.name))
    {
        if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0)
        {
            return false;
        }

        IDSetSwitch(&mIsoSP, nullptr);
        return true;
    }
#endif

    return false;
}

bool MMALDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_DEBUG("%s(%s, %s,\n", __FUNCTION__, dev, name);
    for(int i = 0; i < n; i++)
    {
        LOGF_DEBUG("      value:%f, name: %s,\n", values[i], names[i]);
    }
    LOG_DEBUG(")\n");

    // ignore if not ours
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return false;

    if (INDI::CCD::ISNewNumber(dev, name, values, names, n))
    {
        return true;
    }

    if (!strcmp(name, mGainNP.name))
    {
        IUUpdateNumber(&mGainNP, values, names, n);
        mGainNP.s = IPS_OK;
        IDSetNumber(&mGainNP, nullptr);
        return true;
    }

    return false;
}

bool MMALDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    LOGF_DEBUG("%s(%s, %s,\n", __FUNCTION__, dev, name);
    for(int i = 0; i < n; i++)
    {
        LOGF_DEBUG("      text:%s, name: %s,\n", texts[i], names[i]);
    }
    LOG_DEBUG(")\n");

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool MMALDriver::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                           char *names[], int n)
{
    LOGF_DEBUG("%s(%s, %s,\n", __FUNCTION__, dev, name);
    for(int i = 0; i < n; i++)
    {
        LOGF_DEBUG("      size:%d, blobsize:%d, format:%s, name:%s\n", sizes[i], blobsizes[i], formats[i], names[i]);
    }
    LOG_DEBUG(")\n");

    return INDI::CCD::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void MMALDriver::setupPipeline()
{
    LOG_TEST("entered");

    assert(camera_control->get_camera());

    if (!strcmp(camera_control->get_camera()->getModel(), "imx477"))
    {
        raw_pipe.reset(new JpegPipeline());

        BroadcomPipeline *brcm_pipe = new BroadcomPipeline();
        raw_pipe->daisyChain(brcm_pipe);

        Raw12ToBayer16Pipeline *raw12_pipe = new Raw12ToBayer16Pipeline(brcm_pipe, &chipWrapper);
        brcm_pipe->daisyChain(raw12_pipe);
    }
    else if (!strcmp(camera_control->get_camera()->getModel(), "ov5647"))
    {


        raw_pipe.reset(new JpegPipeline());

        BroadcomPipeline *brcm_pipe = new BroadcomPipeline();
        raw_pipe->daisyChain(brcm_pipe);

        Raw10ToBayer16Pipeline *raw10_pipe = new Raw10ToBayer16Pipeline(brcm_pipe, &chipWrapper);
        // receiver->daisyChain(&raw_writer);
        brcm_pipe->daisyChain(raw10_pipe);
    }
    else if (!strcmp(camera_control->get_camera()->getModel(), "imx219"))
    {
        raw_pipe.reset(new JpegPipeline());

        BroadcomPipeline *brcm_pipe = new BroadcomPipeline();
        raw_pipe->daisyChain(brcm_pipe);

        Raw10ToBayer16Pipeline *raw10_pipe = new Raw10ToBayer16Pipeline(brcm_pipe, &chipWrapper);
        brcm_pipe->daisyChain(raw10_pipe);
    }
    else
    {
        LOGF_WARN("%s: Unknown camera type: %s\n", __FUNCTION__, camera_control->get_camera()->getModel());
        return;
    }
}
