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

#define TMPFILEBASE "/tmp/indipentax.tmp"

PkTriggerCordCCD::PkTriggerCordCCD(const char * name)
{
    snprintf(this->name, 32, "%s", name);
    setDeviceName(this->name);

    InExposure = false;
    InDownload = false;

    setVersion(INDI_PENTAX_VERSION_MAJOR, INDI_PENTAX_VERSION_MINOR);

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

    IUFillText(&DeviceInfoT[0], "MANUFACTURER", "Manufacturer", "");
    IUFillText(&DeviceInfoT[1], "MODEL", "Model", "");
    IUFillText(&DeviceInfoT[2], "FIRMWARE_VERSION", "Firmware", "");
    IUFillText(&DeviceInfoT[3], "SERIAL_NUMBER", "Serial", "");
    IUFillText(&DeviceInfoT[4], "BATTERY", "Battery", "");
    IUFillText(&DeviceInfoT[5], "EXPPROGRAM", "Program", "");
    IUFillText(&DeviceInfoT[6], "UCMODE", "User Mode", "");
    IUFillTextVector(&DeviceInfoTP, DeviceInfoT, NARRAY(DeviceInfoT), getDeviceName(), "DEVICE_INFO", "Device Info", INFO_TAB, IP_RO, 60, IPS_IDLE);
    registerProperty(&DeviceInfoTP, INDI_TEXT);

    IUFillSwitch(&autoFocusS[0], "ON", "On", ISS_OFF);
    IUFillSwitch(&autoFocusS[1], "OFF", "Off", ISS_ON);
    IUFillSwitchVector(&autoFocusSP, autoFocusS, 2, getDeviceName(), "AUTO_FOCUS", "Auto Focus", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&transferFormatS[0], "FORMAT_FITS", "FITS", ISS_ON);
    IUFillSwitch(&transferFormatS[1], "FORMAT_NATIVE", "Native", ISS_OFF);
    IUFillSwitchVector(&transferFormatSP, transferFormatS, 2, getDeviceName(), "CCD_TRANSFER_FORMAT", "Output", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&preserveOriginalS[1], "PRESERVE_ON", "Also Copy Native Image", ISS_OFF);
    IUFillSwitch(&preserveOriginalS[0], "PRESERVE_OFF", "Keep FITS Only", ISS_ON);
    IUFillSwitchVector(&preserveOriginalSP, preserveOriginalS, 2, getDeviceName(), "PRESERVE_ORIGINAL", "Copy Option", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 30, 1, false);

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

        //buildCaptureSwitches();

        defineSwitch(&transferFormatSP);
        defineSwitch(&autoFocusSP);
        if (transferFormatS[0].s == ISS_ON) {
            defineSwitch(&preserveOriginalSP);
        }

        timerID = SetTimer(POLLMS);
    }
    else
    {
        //deleteCaptureSwitches();

        deleteProperty(autoFocusSP.name);
        deleteProperty(transferFormatSP.name);
        deleteProperty(preserveOriginalSP.name);

        rmTimer(timerID);
    }

    return true;
}

/*
void PkTriggerCordCCD::buildCaptureSwitches() {
    buildCaptureSettingSwitch(&mIsoSP,&iso,"ISO","CCD_ISO");
    buildCaptureSettingSwitch(&mApertureSP,&aperture,"Aperture");
    buildCaptureSettingSwitch(&mExpCompSP,&exposurecomp,"Exp Comp");
    buildCaptureSettingSwitch(&mWhiteBalanceSP,&whitebalance,"White Balance");
    buildCaptureSettingSwitch(&mIQualitySP,&imagequality,"Quality");
    buildCaptureSettingSwitch(&mFormatSP,&imageformat,"Format","CAPTURE_FORMAT");
    buildCaptureSettingSwitch(&mStorageWritingSP,&storagewriting, "Write to SD");

    refreshBatteryStatus();
    IUSaveText(&DeviceInfoT[5],exposureprogram.toString().c_str());
    IUSaveText(&DeviceInfoT[6],usercapturesettingsmode.toString().c_str());
    IDSetText(&DeviceInfoTP,nullptr);
}

void PkTriggerCordCCD::deleteCaptureSwitches() {
    if (mIsoSP.nsp > 0) deleteProperty(mIsoSP.name);
    if (mApertureSP.nsp > 0) deleteProperty(mApertureSP.name);
    if (mExpCompSP.nsp > 0) deleteProperty(mExpCompSP.name);
    if (mWhiteBalanceSP.nsp > 0) deleteProperty(mWhiteBalanceSP.name);
    if (mIQualitySP.nsp > 0) deleteProperty(mIQualitySP.name);
    if (mFormatSP.nsp > 0) deleteProperty(mFormatSP.name);
    if (mStorageWritingSP.nsp > 0) deleteProperty(mStorageWritingSP.name);
}
*/

bool PkTriggerCordCCD::Connect()
{
    LOG_INFO("Attempting to connect to the Pentax CCD...");
    char *d = nullptr;
    device = pslr_init(name,d);
    int r;
    if ((r=pslr_connect(device)) ) {
        if ( r != -1 ) {
            LOG_ERROR("Cannot connect to Pentax camera.");
        } else {
            LOG_ERROR("Unknown Pentax camera found.");
        }
        return false;
    }
    pslr_get_status(device, &status);

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
    getCaptureSettingsState();

    //I don't think any of this is needed for the way I'm doing things, but leaving it in until I verify
    float x_pixel_size, y_pixel_size;
    int bit_depth = 16;
    int x_1, y_1, x_2, y_2;

    x_pixel_size = 3.89;
    y_pixel_size = 3.89;

    x_1 = y_1 = 0;
    x_2       = 6000;
    y_2       = 4000;

    bit_depth = 16;
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8; //  this is pixel cameraCount
    nbuf += 512;                                                                  //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}


bool PkTriggerCordCCD::StartExposure(float duration)
{
    if (InExposure)
    {
        LOG_ERROR("Camera is already exposing.");
        return false;
    }
    else {

        InExposure = true;

        //update shutter speed
        PrimaryCCD.setExposureDuration(duration);
        ExposureRequest = duration;
        pslr_rational_t shutter_speed = {duration,1};

        //apply any outstanding capture settings changes
        //pslr_get_status(device, &status);

        uff = USER_FILE_FORMAT_JPEG;
        pslr_set_user_file_format(device, uff);
        quality = status.jpeg_quality;
        pslr_set_jpeg_stars(device, quality);

        //start capture
        gettimeofday(&ExpStart, nullptr);
        LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

        if ( status.exposure_mode ==  PSLR_GUI_EXPOSURE_MODE_B ) {
            if (pslr_get_model_old_bulb_mode(device)) {
                struct timeval prev_time;
                gettimeofday(&prev_time, NULL);
                bulb_old(device, shutter_speed, prev_time);
            } else {
                need_bulb_new_cleanup = true;
                bulb_new(device, shutter_speed);
            }
        } else {
            DPRINT("not bulb\n");
            //if (!settings.one_push_bracketing.value || bracket_index == 0) {
            if (1) {
                pslr_shutter(device);
            } else {
                // TODO: fix waiting time
                sleep_sec(1);
            }
        }

        user_file_format_t ufft = *get_file_format_t(uff);
        char * output_file = TMPFILEBASE;
        fd = open_file(output_file, 1, ufft);
        pslr_get_status(device, &status);

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
        if ( !save_buffer(device, 0, fd, &status, uff, quality )) {

            InDownload = false;
            InExposure = false;
            pslr_delete_buffer(device, 0);
            if (fd != 1) {
                close(fd);
            }
            if (need_bulb_new_cleanup) {
                bulb_new_cleanup(device);
            }
            grabImage();
            ExposureComplete(&PrimaryCCD);
        } else if (!InDownload && isDebug()) {
            IDLog("Still waiting for download...");
        }

        timeleft = CalcTimeLeft();

        if (!InDownload) {
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
                        timeleft=0;
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
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

bool PkTriggerCordCCD::grabImage()
{
    char tmpfile[256];

    uint8_t * memptr = PrimaryCCD.getFrameBuffer();
    size_t memsize = 0;
    int naxis = 2, w = 0, h = 0, bpp = 8;


    if (uff==USER_FILE_FORMAT_JPEG)
    {
        snprintf(tmpfile, 256, "%s-0001.jpg", TMPFILEBASE);
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
        if (uff==USER_FILE_FORMAT_DNG) {
            snprintf(tmpfile, 256, "%s-0001.dng", TMPFILEBASE);
        }
        else {
            snprintf(tmpfile, 256, "%s-0001.pef", TMPFILEBASE);
        }
        char bayer_pattern[8] = {};

        if (read_libraw(tmpfile, &memptr, &memsize, &naxis, &w, &h, &bpp, bayer_pattern))
        {
            LOG_ERROR("Exposure failed to parse raw image.");
            unlink(tmpfile);
            return false;
        }

        LOGF_DEBUG("read_libraw: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d) bayer pattern (%s)",
                   memsize, naxis, w, h, bpp, bayer_pattern);

        unlink(tmpfile);

        IUSaveText(&BayerT[2], bayer_pattern);
        IDSetText(&BayerTP, nullptr);
        SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
    }

    if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
        LOGF_WARN("Camera image size (%dx%d) is less than requested size (%d,%d). Purging configuration and update frame size to match camera size.", w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

    PrimaryCCD.setFrame(0, 0, w, h);
    PrimaryCCD.setFrameBuffer(memptr);
    PrimaryCCD.setFrameBufferSize(memsize, false);
    PrimaryCCD.setResolution(w, h);
    PrimaryCCD.setNAxis(naxis);
    PrimaryCCD.setBPP(bpp);

    //set correct file extension, depending on whether native or fits is selected
    if (transferFormatS[0].s == ISS_ON)
    {
        PrimaryCCD.setImageExtension("fits");
    }
    else
    {
        PrimaryCCD.setImageExtension(getFormatFileExtension(uff));
    }

    //(re)move temporary image file
    if (preserveOriginalS[1].s == ISS_ON) {
        char ts[32];
        struct timeval now;
        struct tm * tp;
        time_t t = gettimeofday(&now, nullptr);
        tp = localtime(&t);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%S", tp);
        std::string prefix = getUploadFilePrefix();
        prefix = std::regex_replace(prefix, std::regex("XXX"), string(ts));
        char newname[255];
        snprintf(newname, 255, "%s.%s",prefix.c_str(),getFormatFileExtension(uff));
        if (std::rename(tmpfile, newname)) {
            LOGF_ERROR("File system error prevented saving original image to %s.  Saved to %s instead.", newname,tmpfile);
        }
        else {
            LOGF_INFO("Saved original image to %s.", newname);
        }
    }
    else {
        std::remove(tmpfile);
    }

    return true;
}


ISwitch * PkTriggerCordCCD::create_switch(const char * basestr, std::vector<string> options, int setidx)
{

    ISwitch * sw     = static_cast<ISwitch *>(calloc(sizeof(ISwitch), options.size()));
    ISwitch * one_sw = sw;

    char sw_name[MAXINDINAME];
    char sw_label[MAXINDILABEL];
    ISState sw_state;

    for (int i = 0; i < (int)options.size(); i++)
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

    if (!strcmp(name, autoFocusSP.name)) {
        IUUpdateSwitch(&autoFocusSP, states, names, n);
        autoFocusSP.s = IPS_OK;
        IDSetSwitch(&autoFocusSP, nullptr);
    }
    else if (!strcmp(name, transferFormatSP.name)) {
        IUUpdateSwitch(&transferFormatSP, states, names, n);
        transferFormatSP.s = IPS_OK;
        IDSetSwitch(&transferFormatSP, nullptr);
        if (transferFormatS[0].s == ISS_ON) {
            defineSwitch(&preserveOriginalSP);
        } else {
            deleteProperty(preserveOriginalSP.name);
        }
    }
    else if (!strcmp(name, preserveOriginalSP.name)) {
        IUUpdateSwitch(&preserveOriginalSP, states, names, n);
        preserveOriginalSP.s = IPS_OK;
        IDSetSwitch(&preserveOriginalSP, nullptr);
    }
    else if (!strcmp(name, mIsoSP.name)) {
        updateCaptureSettingSwitch(&mIsoSP,states,names,n);
    }
    else if (!strcmp(name, mApertureSP.name)) {
        updateCaptureSettingSwitch(&mApertureSP,states,names,n);
    }
    else if (!strcmp(name, mExpCompSP.name)) {
        updateCaptureSettingSwitch(&mExpCompSP,states,names,n);
    }
    else if (!strcmp(name, mWhiteBalanceSP.name)) {
        updateCaptureSettingSwitch(&mWhiteBalanceSP,states,names,n);
    }
    else if (!strcmp(name, mIQualitySP.name)) {
        updateCaptureSettingSwitch(&mIQualitySP,states,names,n);
    }
    else if (!strcmp(name, mFormatSP.name)) {
        updateCaptureSettingSwitch(&mFormatSP,states,names,n);
    }
    else if (!strcmp(name, mStorageWritingSP.name)) {
        updateCaptureSettingSwitch(&mStorageWritingSP,states,names,n);
    }
    else {
        return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
    }
    return true;
}

void PkTriggerCordCCD::updateCaptureSettingSwitch(ISwitchVectorProperty * sw, ISState * states, char * names[], int n) {
    IUUpdateSwitch(sw, states, names, n);
    sw->s = IPS_OK;
    IDSetSwitch(sw, nullptr);
}

bool PkTriggerCordCCD::saveConfigItems(FILE * fp) {

    for (auto sw : std::vector<ISwitchVectorProperty*>{&mIsoSP,&mApertureSP,&mExpCompSP,&mWhiteBalanceSP,&mIQualitySP,&mFormatSP,&mStorageWritingSP}) {
        if (sw->nsp>0) IUSaveConfigSwitch(fp, sw);
    }

    // Save regular CCD properties
    return INDI::CCD::saveConfigItems(fp);
}

void PkTriggerCordCCD::addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status = 0;

    if (mIsoSP.nsp > 0)
    {
        ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
        if (onISO)
        {
            int isoSpeed = atoi(onISO->label);
            if (isoSpeed > 0)
                fits_update_key_s(fptr, TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
        }
    }
}
/*
void PkTriggerCordCCD::buildCaptureSettingSwitch(ISwitchVectorProperty *control, CaptureSetting *setting, const char *label, const char *name) {
    std::vector<string> optionList;
    int set_idx=0, i=0;
    for (const auto s : setting->getAvailableSettings()) {
        optionList.push_back(s->getValue().toString());
        if (s->getValue().toString() == setting->getValue().toString()) set_idx=i;
        i++;
    }

    if (optionList.size() > 0)
    {
        IUFillSwitchVector(control, create_switch(setting->getName().c_str(), optionList, set_idx),
                           optionList.size(), getDeviceName(),
                           name ? name : setting->getName().c_str(),
                           label ? label : setting->getName().c_str(),
                           IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        defineSwitch(control);
    }
}
*/
void PkTriggerCordCCD::getCaptureSettingsState() {

}

string PkTriggerCordCCD::getUploadFilePrefix() {
    return UploadSettingsT[UPLOAD_DIR].text + string("/") + UploadSettingsT[UPLOAD_PREFIX].text;
}

const char * PkTriggerCordCCD::getFormatFileExtension(user_file_format format) {
    if (format==USER_FILE_FORMAT_JPEG) {
        return "jpg";
    }
    else if (format==USER_FILE_FORMAT_DNG) {
        return "raw";
    }
    else {
        return "pef";
    }
}
