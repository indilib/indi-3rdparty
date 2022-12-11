/*
 Pentax CCD Driver for Indi (using PkTriggerCord)
 Copyright (C) 2020 Karl Rees

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


#include "pktriggercord_ccd.h"
#include "pslr.h"
#include <indimacros.h>

#define MINISO 100
#define MAXISO 102400

#define TMPFILEBASE (char *)"/tmp/indipentax.tmp"

PkTriggerCordCCD::PkTriggerCordCCD(const char * name)
{
    snprintf(this->name, 32, "%s", name);
    char displayname[32];
    snprintf(displayname, 32, "PENTAX %s (MSC)", name);
    setDeviceName(displayname);

    InExposure = false;
    InDownload = false;

    setVersion(INDI_PENTAX_VERSION_MAJOR, INDI_PENTAX_VERSION_MINOR);
    LOG_INFO("The Pentax camera driver for MSC mode uses PkTriggerCord, courtesy of Andras Salamon.  See https://pktriggercord.melda.info");
}

PkTriggerCordCCD::~PkTriggerCordCCD()
{
}

const char *PkTriggerCordCCD::getDefaultName()
{
    return "Pentax DSLR";
}

bool PkTriggerCordCCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillText(&DeviceInfoT[0], "MODEL", "Model", name);
    IUFillText(&DeviceInfoT[1], "FIRMWARE_VERSION", "Firmware", "");
    IUFillText(&DeviceInfoT[2], "BATTERY", "Battery", "");
    IUFillText(&DeviceInfoT[3], "EXPPROGRAM", "Program", "");
    IUFillText(&DeviceInfoT[4], "UCMODE", "User Mode", "");
    IUFillText(&DeviceInfoT[5], "SCENEMODE", "Scene Mode", "");

    IUFillTextVector(&DeviceInfoTP, DeviceInfoT, NARRAY(DeviceInfoT), getDeviceName(), "DEVICE_INFO", "Device Info", INFO_TAB,
                     IP_RO, 60, IPS_IDLE);
    registerProperty(&DeviceInfoTP, INDI_TEXT);

    IUFillSwitch(&autoFocusS[0], "ON", "On", ISS_OFF);
    IUFillSwitch(&autoFocusS[1], "OFF", "Off", ISS_ON);
    IUFillSwitchVector(&autoFocusSP, autoFocusS, 2, getDeviceName(), "AUTO_FOCUS", "Auto Focus", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);
/*
    IUFillSwitch(&transferFormatS[0], "FORMAT_FITS", "FITS", ISS_ON);
    IUFillSwitch(&transferFormatS[1], "FORMAT_NATIVE", "Native", ISS_OFF);
    IUFillSwitchVector(&transferFormatSP, transferFormatS, 2, getDeviceName(), "CCD_TRANSFER_FORMAT", "Output", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
*/

    IUFillSwitch(&preserveOriginalS[1], "PRESERVE_ON", "Also Copy Native Image", ISS_OFF);
    IUFillSwitch(&preserveOriginalS[0], "PRESERVE_OFF", "Keep FITS Only", ISS_ON);
    IUFillSwitchVector(&preserveOriginalSP, preserveOriginalS, 2, getDeviceName(), "PRESERVE_ORIGINAL", "Copy Option",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.0001, 7200, 1, false);

    IUSaveText(&BayerT[2], "RGGB");

    PrimaryCCD.getCCDInfo()->p = IP_RW;

    uint32_t cap = CCD_HAS_BAYER;
    SetCCDCapability(cap);

    addConfigurationControl();
    addDebugControl();
    return true;
}

void PkTriggerCordCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool PkTriggerCordCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        setupParams();

        buildCaptureSwitches();

        // defineProperty(&transferFormatSP);
        defineProperty(&autoFocusSP);
        //if (transferFormatS[0].s == ISS_ON)        
        if (EncodeFormatSP[FORMAT_FITS].getState() == ISS_ON)
        {
            defineProperty(&preserveOriginalSP);
        }

        timerID = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteCaptureSwitches();

        deleteProperty(autoFocusSP.name);
        // deleteProperty(transferFormatSP.name);
        deleteProperty(preserveOriginalSP.name);

        rmTimer(timerID);
    }

    return true;
}


void PkTriggerCordCCD::buildCaptureSwitches()
{

    string iso[] = {"100", "200", "400", "800", "1000", "1600", "3200", "6400", "12800", "25600", "51200", "102400"};
    string aperture[] = {    "1.0", "1.1", "1.2", "1.4", "1.6", "1.7", "1.8",
                             "2.0", "2.2", "2.4", "2.5", "2.8", "3.2", "3.5",
                             "4.0", "4.5", "5.0", "5.6", "6.3", "6.7", "7.1",
                             "8.0", "9.0", "9.5", "10.0", "11.0", "13.0", "14.0",
                             "16.0", "18.0", "19.0", "20.0", "22.0", "25.0", "28.0",
                             "32.0", "36.0", "40.0", "45.0", "51.0", "57.0"
                        };
    string exposurecomp1_3[] = {"-3.0", "-2.7", "-2.3", "-2.0", "-1.7", "-1.3", "-1.0", "-0.7", "-0.3", "0", "0.3", "0.7", "1.0", "1.3", "1.7", "2.0", "2.3", "2.7", "3.0"};
    string exposurecomp1_2[] = { "-3.0", "-2.5", "-2.0", "-1.5", "-1.0", "-0.5", "0", "0.5", "1.0", "1.5", "2.0", "3.0"};

    string whitebalance[] = {"Auto", "Daylight", "Shade", "Cloudy", "Fluorescent_D", "Fluorescent_N", "Fluorescent_W", "Fluorescent_L", "Tungsten", "Flash", "Manual", "Manual2", "Manual3", "Kelvin1", "Kelvin2", "Kelvin3", "CTE", "MultiAuto"};
    string imagequality[] = {"1", "2", "3", "4"};
    string imageformat[] = {"JPEG", "PEF", "DNG"};

    buildCaptureSettingSwitch(&mIsoSP, iso, NARRAY(iso), "ISO", "CCD_ISO", to_string(status.current_iso));
    char current[5];
    sprintf(current, "%.1f", (float)status.current_aperture.nom / status.current_aperture.denom);
    buildCaptureSettingSwitch(&mApertureSP, aperture, NARRAY(aperture), "Aperture", "CCD_APERTURE", string(current));
    buildCaptureSettingSwitch(&mWhiteBalanceSP, whitebalance, NARRAY(whitebalance), "White Balance", "CCD_WB",
                              string(pslr_get_white_balance_mode_str((pslr_white_balance_mode_t)status.white_balance_mode)));
    buildCaptureSettingSwitch(&mIQualitySP, imagequality, pslr_get_model_max_jpeg_stars(device), "Quality", "CAPTURE_QUALITY",
                              to_string(status.jpeg_quality));
    sprintf(current, "%.1f", (float)status.ec.nom / status.ec.denom);
    if (status.custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2)
        buildCaptureSettingSwitch(&mExpCompSP, exposurecomp1_2, NARRAY(exposurecomp1_2), "Exp Comp", "CCD_EC", string(current));
    else
        buildCaptureSettingSwitch(&mExpCompSP, exposurecomp1_3, NARRAY(exposurecomp1_3), "Exp Comp", "CCD_EC", string(current));

    string f = "JPEG";
    if (uff == USER_FILE_FORMAT_DNG)
    {
        f = "DNG";
    }
    else if (uff == USER_FILE_FORMAT_PEF)
    {
        f = "PEF";
    }
    buildCaptureSettingSwitch(&mFormatSP, imageformat, NARRAY(imageformat), "Format", "CAPTURE_FORMAT", f);

    refreshBatteryStatus();

    char exposuremode[10];
    char usermode[10];
    char firmware[16];

    sprintf(exposuremode, "%d", status.exposure_mode);
    sprintf(usermode, "%d", status.user_mode_flag);

    pslr_get_dspinfo( (pslr_handle_t *)device, firmware );

    IUSaveText(&DeviceInfoT[1], firmware);
    IUSaveText(&DeviceInfoT[3], exposuremode);
    IUSaveText(&DeviceInfoT[4], usermode);
    IUSaveText(&DeviceInfoT[5], pslr_get_scene_mode_str((pslr_scene_mode_t)status.scene_mode));

    IDSetText(&DeviceInfoTP, nullptr);
}

void PkTriggerCordCCD::deleteCaptureSwitches()
{
    if (mIsoSP.nsp > 0) deleteProperty(mIsoSP.name);
    if (mApertureSP.nsp > 0) deleteProperty(mApertureSP.name);
    if (mExpCompSP.nsp > 0) deleteProperty(mExpCompSP.name);
    if (mWhiteBalanceSP.nsp > 0) deleteProperty(mWhiteBalanceSP.name);
    if (mIQualitySP.nsp > 0) deleteProperty(mIQualitySP.name);
    // if (mFormatSP.nsp > 0) deleteProperty(mFormatSP.name);
}


void PkTriggerCordCCD::buildCaptureSettingSwitch(ISwitchVectorProperty *control, string optionList[], size_t numOptions,
        const char *label, const char *name, string currentsetting)
{

    int set_idx = 0;
    if (numOptions > 0)
    {
        for (size_t i = 0; i < numOptions; i++)
        {
            if (optionList[i] == currentsetting) set_idx = i;
        }

        IUFillSwitchVector(control, create_switch(name, optionList, numOptions, set_idx),
                           numOptions, getDeviceName(), name, label,
                           IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        defineProperty(control);
    }
}

bool PkTriggerCordCCD::Connect()
{
    char *d = nullptr;
    device = pslr_init(name, d);
    if (! device)
    {
        LOG_ERROR("Cannot connect to Pentax camera.");
        return false;
    }
    int r = pslr_connect(device);
    if (r != 0)
    {
        if ( r == -1 )
        {
            LOG_ERROR("Unknown Pentax camera found.");
        }
        else
        {
            LOG_ERROR("Cannot connect to Pentax camera.");
        }
        return false;
    }
    InExposure = false;
    InDownload = false;
    LOG_INFO("Connected to Pentax camera in MSC mode.");
    return true;
}

bool PkTriggerCordCCD::Disconnect()
{
    pslr_disconnect(device);
    pslr_shutdown(device);
    return true;
}

bool PkTriggerCordCCD::setupParams()
{
    if (!getCaptureSettingsState()) return false;
    uff = pslr_get_user_file_format(&status);
    return true;
}

bool PkTriggerCordCCD::shutterPress(pslr_rational_t shutter_speed)
{
    if ( status.exposure_mode ==  PSLR_GUI_EXPOSURE_MODE_B )
    {
        if (pslr_get_model_old_bulb_mode(device))
        {
            LOG_DEBUG("old bulb\n");
            struct timeval prev_time;
            gettimeofday(&prev_time, NULL);
            bulb_old(device, shutter_speed, prev_time);
        }
        else
        {
            need_bulb_new_cleanup = true;
            bulb_new(device, shutter_speed);
            LOG_DEBUG("new bulb\n");
        }
    }
    else
    {
        LOG_DEBUG("not bulb\n");
        if (1)
        {
            pslr_shutter(device);
        }
        else
        {
            // TODO: fix waiting time
            usleep(999999);
            return false;
        }
    }
    LOG_DEBUG("Shutter pressed.");
    pslr_get_status(device, &status);

    user_file_format_t ufft = *pslr_get_user_file_format_t(uff);
    char * output_file = TMPFILEBASE;
    int fd = open_file(output_file, 1, ufft);

    int cnt = 0;
    while ( save_buffer(device, 0, fd, &status, uff, quality ) )
    {
        LOGF_DEBUG("Saved buffer (%d)", cnt++);
    }

    pslr_delete_buffer(device, 0);
    if (fd != 1)
    {
        close(fd);
    }
    if (need_bulb_new_cleanup)
    {
        bulb_new_cleanup(device);
    }

    return 1;
}


bool PkTriggerCordCCD::StartExposure(float duration)
{
    if (InExposure)
    {
        LOG_ERROR("Camera is already exposing.");
        return false;
    }
    else
    {
        //just need to check if we changed exposure modes and are still connected before proceeding here
        if (!getCaptureSettingsState())
        {
            LOG_INFO("Could not get camera state.  Are we still connected?");
            return false;
        }

        //check if duration is valid
        if (!duration)
        {
            LOG_INFO("Shutter speed must be greater than 0.");
            return false;
        }
        else if (( status.exposure_mode ==  PSLR_GUI_EXPOSURE_MODE_B ) && (duration < 1))
        {
            LOG_INFO("Shutter speed must be at least 1 in bulb mode.");
            return false;
        }

        InExposure = true;

        //update shutter speed
        if ( status.exposure_mode !=  PSLR_GUI_EXPOSURE_MODE_B )
        {
            if (duration > 30)
            {
                duration = 30;
                LOG_INFO("Exposures longer than 30 seconds not supported in current mode.  Setting exposure time to 30 seconds.  Change camera to bulb mode for longer exposures.");
            }
            else
            {
                LOGF_INFO("Only pre-defined shutter speeds are supported in current mode.  The camera will select the pre-defined shutter speed that most closely matches %f.",
                          duration);
            }
        }
        PrimaryCCD.setExposureDuration(duration);
        ExposureRequest = duration;
        float F = duration;
        pslr_rational_t shutter_speed;
        shutter_speed.denom = 1;
        int i = 0;
        if (F < 5)
        {
            while ((rintf(F) != F) && (i++ < 4))
            {
                F = F * 10;
                shutter_speed.denom *= 10;
            }
            shutter_speed.nom = F;
        }
        else
        {
            shutter_speed.nom = F;
        }
        //pslr_rational_t shutter_speed = {(int)(duration*100),100};
        //Doesn't look like we need to actually set the shutter speed in bulb mode
        if ( status.exposure_mode !=  PSLR_GUI_EXPOSURE_MODE_B )
        {
            if (duration != (float)status.current_shutter_speed.nom / status.current_shutter_speed.denom)
            {
                pslr_set_shutter(device, shutter_speed);
            }
        }

        if (autoFocusS[0].s == ISS_ON) pslr_focus(device);

        //start capture
        gettimeofday(&ExpStart, nullptr);
        LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

        shutter_result = std::async(std::launch::async, &PkTriggerCordCCD::shutterPress, this, shutter_speed);

        return true;
    }
}

bool PkTriggerCordCCD::AbortExposure()
{

    return true;
}

bool PkTriggerCordCCD::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{
    INDI::CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

    if (fType == imageFrameType)
        return true;

    switch (imageFrameType)
    {
        case INDI::CCDChip::BIAS_FRAME:
        case INDI::CCDChip::DARK_FRAME:
            /**********************************************************
            *
            *
            *
            *  IMPORTANT: Put here your CCD Frame type here
            *  BIAS and DARK are taken with shutter closed, so _usually_
            *  most CCD this is a call to let the CCD know next exposure shutter
            *  must be closed. Customize as appropiate for the hardware
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to set frame type to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;

        case INDI::CCDChip::LIGHT_FRAME:
        case INDI::CCDChip::FLAT_FRAME:
            /**********************************************************
            *
            *
            *
            *  IMPORTANT: Put here your CCD Frame type here
            *  LIGHT and FLAT are taken with shutter open, so _usually_
            *  most CCD this is a call to let the CCD know next exposure shutter
            *  must be open. Customize as appropiate for the hardware
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to set frame type to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;
    }

    PrimaryCCD.setFrameType(fType);

    return true;
}


float PkTriggerCordCCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;

    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;

    //compensate for delay when starting bulb timer
    if (status.exposure_mode ==  PSLR_GUI_EXPOSURE_MODE_B )
    {
        timeleft = (timeleft + 5 < ExposureRequest) ? timeleft + 5 : ExposureRequest;
    }
    return timeleft;
}

void PkTriggerCordCCD::TimerHit()
{
    int timerID = -1;
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        if (!InDownload)
        {
            if (timeleft < 1.0)
            {
                if (timeleft > 0.25)
                {
                    //  a quarter of a second or more
                    //  just set a tighter timer
                    timerID = SetTimer(250);
                }
                else
                {
                    if (timeleft > 0.07)
                    {
                        //  use an even tighter timer
                        timerID = SetTimer(50);
                    }
                    else
                    {
                        LOG_INFO("Capture finished.  Waiting for image download...");
                        InDownload = true;
                        timeleft = 0;
                        PrimaryCCD.setExposureLeft(0);
                    }
                }
            }
            else
            {
                if (isDebug())
                {
                    IDLog("With time left %ld\n", timeleft);
                    IDLog("image not yet ready....\n");
                }
                PrimaryCCD.setExposureLeft(timeleft);
            }
        }

        std::chrono::milliseconds span (100);
        if ( shutter_result.wait_for(span) != std::future_status::timeout)
        {
            bool result = shutter_result.get();
            INDI_UNUSED(result);
            InDownload = false;
            InExposure = false;

            grabImage();
            ExposureComplete(&PrimaryCCD);
        }
        else if (InDownload && isDebug())
        {
            IDLog("Still waiting for download...\n");
        }
    }

    if (timerID == -1)
        SetTimer(getCurrentPollingPeriod());
    return;
}

bool PkTriggerCordCCD::grabImage()
{
    //set correct tmpfile location
    char tmpfile[256];
    if (uff == USER_FILE_FORMAT_JPEG)
    {
        snprintf(tmpfile, 256, "%s-0001.jpg", TMPFILEBASE);
    }
    else if (uff == USER_FILE_FORMAT_DNG)
    {
        snprintf(tmpfile, 256, "%s-0001.dng", TMPFILEBASE);
    }
    else
    {
        snprintf(tmpfile, 256, "%s-0001.pef", TMPFILEBASE);
    }
    LOGF_DEBUG("Reading temp file from: %s", tmpfile);


    // fits handling code
    // if (transferFormatS[0].s == ISS_ON)    
    if ( EncodeFormatSP[FORMAT_FITS].s == ISS_ON )
    {
        PrimaryCCD.setImageExtension("fits");
        uint8_t * memptr = PrimaryCCD.getFrameBuffer();
        size_t memsize = 0;
        int naxis = 2, w = 0, h = 0, bpp = 8;

        if (uff == USER_FILE_FORMAT_JPEG)
        {
            if (read_jpeg(tmpfile, &memptr, &memsize, &naxis, &w, &h))
            {
                LOG_ERROR("Exposure failed to parse jpeg.");
                unlink(tmpfile);
                return false;
            }


            LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis,
                       w, h, bpp);

            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }
        else
        {
            char bayer_pattern[8] = {};

            if (read_libraw(tmpfile, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
            {
                LOG_ERROR("Exposure failed to parse raw image.");
                unlink(tmpfile);
                return false;
            }

            LOGF_DEBUG("read_libraw: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d) bayer pattern (%s)",
                       memsize, naxis, w, h, bpp, bayer_pattern);

            IUSaveText(&BayerT[2], bayer_pattern);
            IDSetText(&BayerTP, nullptr);
            SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
        }

        if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
            LOGF_WARN("Camera image size (%dx%d) is different than requested size (%d,%d). Purging configuration and updating frame size to match camera size.",
                      w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

        PrimaryCCD.setFrame(0, 0, w, h);
        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(memsize, false);
        PrimaryCCD.setResolution(w, h);
        PrimaryCCD.setNAxis(naxis);
        PrimaryCCD.setBPP(bpp);

        if (preserveOriginalS[1].s == ISS_ON)
        {
            char ts[32];
            struct tm * tp;
            time_t t;
            time(&t);
            tp = localtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
            std::string prefix = getUploadFilePrefix();
            prefix = std::regex_replace(prefix, std::regex("XXX"), string(ts));
            char newname[255];
            snprintf(newname, 255, "%s.%s", prefix.c_str(), getFormatFileExtension(uff));
            if (std::rename(tmpfile, newname))
            {
                LOGF_ERROR("File system error prevented saving original image to %s.  Saved to %s instead.", newname, tmpfile);
            }
            else
            {
                LOGF_INFO("Saved original image to %s.", newname);
            }
        }
        else
        {
            std::remove(tmpfile);
        }

    }
    // native handling code
    else
    {
        PrimaryCCD.setImageExtension(getFormatFileExtension(uff));

        FILE* f = fopen(tmpfile, "r");
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        rewind(f);

        PrimaryCCD.setFrameBufferSize(size);
        char * memptr = (char *)PrimaryCCD.getFrameBuffer();
        size_t readSize = fread(memptr, sizeof(char), size, f);
        if (readSize != size)
        {
            LOGF_ERROR("Error, %u bytes of data read instead of %u.", readSize, size);
        }
        PrimaryCCD.setFrameBuffer((unsigned char *)memptr);
        fclose(f);
        LOG_DEBUG("Copied to frame buffer.  Leaving temp file for debug purposes.");
        if (!isDebug())
        {
            std::remove(tmpfile);
        }
    }

    return true;
}


ISwitch * PkTriggerCordCCD::create_switch(const char * basestr, string options[], size_t numOptions, int setidx)
{

    ISwitch * sw     = static_cast<ISwitch *>(calloc(sizeof(ISwitch), numOptions));
    ISwitch * one_sw = sw;

    char sw_name[MAXINDINAME];
    char sw_label[MAXINDILABEL];
    ISState sw_state;

    for (int i = 0; i < (int)numOptions; i++)
    {
        snprintf(sw_name, MAXINDINAME, "%s%d", basestr, i);
        strncpy(sw_label, options[i].c_str(), MAXINDILABEL);
        sw_state = (i == setidx) ? ISS_ON : ISS_OFF;

        IUFillSwitch(one_sw++, sw_name, sw_label, sw_state);
    }

    return sw;
}

bool PkTriggerCordCCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{

    if (!strcmp(name, autoFocusSP.name))
    {
        IUUpdateSwitch(&autoFocusSP, states, names, n);
        autoFocusSP.s = IPS_OK;
        IDSetSwitch(&autoFocusSP, nullptr);
    }
 /*
    else if (!strcmp(name, transferFormatSP.name))
    {
        IUUpdateSwitch(&transferFormatSP, states, names, n);
        transferFormatSP.s = IPS_OK;
        IDSetSwitch(&transferFormatSP, nullptr);
        if (transferFormatS[0].s == ISS_ON)
        {
            defineProperty(&preserveOriginalSP);
        }
        else
        {
            deleteProperty(preserveOriginalSP.name);
        }
    }
    */
    else if (!strcmp(name, preserveOriginalSP.name))
    {
        IUUpdateSwitch(&preserveOriginalSP, states, names, n);
        preserveOriginalSP.s = IPS_OK;
        IDSetSwitch(&preserveOriginalSP, nullptr);
    }
    else if (!strcmp(name, mIsoSP.name))
    {
        updateCaptureSettingSwitch(&mIsoSP, states, names, n);
        pslr_set_iso(device, atoi(IUFindOnSwitch(&mIsoSP)->label), MINISO, MAXISO);
        LOG_WARN("Unfortunately, changing the ISO does not appear to work currently on some (all?) models in MSC mode.  You may need to change manually.");
    }
    else if (!strcmp(name, mApertureSP.name))
    {
        updateCaptureSettingSwitch(&mApertureSP, states, names, n);
        pslr_rational_t ap = {(int)(atof(IUFindOnSwitch(&mApertureSP)->label) * 10), 10};
        pslr_set_aperture(device, ap);
    }
    else if (!strcmp(name, mExpCompSP.name))
    {
        updateCaptureSettingSwitch(&mExpCompSP, states, names, n);
        pslr_rational_t ec = {(int)(atof(IUFindOnSwitch(&mExpCompSP)->label) * 10), 10};
        pslr_set_expose_compensation( device, ec );
        LOG_WARN("Unfortunately, changing the exposure compensation does not work currently on some (all?) models in MSC mode.  You may need to change manually.");
    }
    else if (!strcmp(name, mWhiteBalanceSP.name))
    {
        updateCaptureSettingSwitch(&mWhiteBalanceSP, states, names, n);
        pslr_white_balance_mode_t white_balance_mode = pslr_get_white_balance_mode(IUFindOnSwitch(&mWhiteBalanceSP)->label);
        if ( int(white_balance_mode) == -1 )
        {
            LOG_WARN("Could not set desired white balance: Invalid setting for current camera mode.");
        }
        else
        {
            pslr_set_white_balance( device, white_balance_mode );
        }
    }
    else if (!strcmp(name, mIQualitySP.name))
    {
        updateCaptureSettingSwitch(&mIQualitySP, states, names, n);
        pslr_set_jpeg_stars(device, atoi(IUFindOnSwitch(&mIQualitySP)->label));
    }
    else if (!strcmp(name, mFormatSP.name))
    {
        updateCaptureSettingSwitch(&mFormatSP, states, names, n);
        char *f = IUFindOnSwitch(&mFormatSP)->label;
        if (!strcmp(f, "DNG"))
        {
            uff = USER_FILE_FORMAT_DNG;
        }
        else if (!strcmp(f, "PEF"))
        {
            uff = USER_FILE_FORMAT_PEF;
        }
        else
        {
            uff = USER_FILE_FORMAT_JPEG;
        }
        pslr_set_user_file_format(device, uff);
    }
    else
    {
        return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
    }
    if (!getCaptureSettingsState()) return false;
    return true;
}

void PkTriggerCordCCD::updateCaptureSettingSwitch(ISwitchVectorProperty * sw, ISState * states, char * names[], int n)
{
    IUUpdateSwitch(sw, states, names, n);
    sw->s = IPS_OK;
    IDSetSwitch(sw, nullptr);
}

bool PkTriggerCordCCD::saveConfigItems(FILE * fp)
{

    for (auto sw : std::vector<ISwitchVectorProperty*> {&mIsoSP, &mApertureSP, &mExpCompSP, &mWhiteBalanceSP, &mIQualitySP, &mFormatSP})
    {
        if (sw->nsp > 0) IUSaveConfigSwitch(fp, sw);
    }

    // Save regular CCD properties
    return INDI::CCD::saveConfigItems(fp);
}

void PkTriggerCordCCD::addFITSKeywords(INDI::CCDChip * targetChip)
{
    INDI::CCD::addFITSKeywords(targetChip);

    int status = 0;

    if (mIsoSP.nsp > 0)
    {
        ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
        if (onISO)
        {
            int isoSpeed = atoi(onISO->label);
            if (isoSpeed > 0)
                fits_update_key_s(*targetChip->fitsFilePointer(), TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
        }
    }
}

// returns true if still connected
bool PkTriggerCordCCD::getCaptureSettingsState()
{
    pslr_get_status(device, &status);
    //assume that if we don't see battery info in status, the camera is disconnected
    if (!status.battery_1 && !status.battery_2 && !status.battery_3 && !status.battery_4)
    {
        if (Disconnect())
        {
            setConnected(false, IPS_IDLE);
            updateProperties();
        }
        LOG_ERROR("Error: the camera appears to no longer be connected.");
        return false;
    }
    else return true;
}

string PkTriggerCordCCD::getUploadFilePrefix()
{
    return UploadSettingsT[UPLOAD_DIR].text + string("/") + UploadSettingsT[UPLOAD_PREFIX].text;
}

const char * PkTriggerCordCCD::getFormatFileExtension(user_file_format format)
{
    if (format == USER_FILE_FORMAT_JPEG)
    {
        return "jpg";
    }
    else if (format == USER_FILE_FORMAT_DNG)
    {
        return "raw";
    }
    else
    {
        return "pef";
    }
}

void PkTriggerCordCCD::refreshBatteryStatus()
{
    char batterylevel[25];
    sprintf(batterylevel, "%.2fV %.2fV %.2fV %.2fV\n", 0.01 * status.battery_1, 0.01 * status.battery_2,
            0.01 * status.battery_3, 0.01 * status.battery_4);
    IUSaveText(&DeviceInfoT[2], batterylevel);
    IDSetText(&DeviceInfoTP, nullptr);
}
