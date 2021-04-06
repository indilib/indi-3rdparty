/*
 Pentax CCD Driver for Indi (using Ricoh Camera SDK)
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


#include "pentax_ccd.h"


PentaxCCD::PentaxCCD(std::shared_ptr<CameraDevice> camera)
{
    this->device = camera;
    snprintf(this->name, 32, "%s (PTP)", camera->getModel().c_str());
    setDeviceName(this->name);
    InExposure = false;
    InDownload = false;

    setVersion(INDI_PENTAX_VERSION_MAJOR, INDI_PENTAX_VERSION_MINOR);
    LOG_INFO("The Pentax camera driver for PTP mode uses Ricoh Camera SDK, courtesy of Ricoh Company, Ltd.  See https://api.ricoh/products/camera-sdk.");
}

PentaxCCD::~PentaxCCD()
{
}

const char *PentaxCCD::getDefaultName()
{
    return "Pentax DSLR";
}

bool PentaxCCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillText(&DeviceInfoT[0], "MANUFACTURER", "Manufacturer", device->getManufacturer().c_str());
    IUFillText(&DeviceInfoT[1], "MODEL", "Model", device->getModel().c_str());
    IUFillText(&DeviceInfoT[2], "FIRMWARE_VERSION", "Firmware", device->getFirmwareVersion().c_str());
    IUFillText(&DeviceInfoT[3], "SERIAL_NUMBER", "Serial", device->getSerialNumber().c_str());
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

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0, 30, 1, false);

    IUSaveText(&BayerT[2], "RGGB");

    PrimaryCCD.getCCDInfo()->p = IP_RW;

    uint32_t cap = CCD_HAS_BAYER | CCD_HAS_STREAMING;
    SetCCDCapability(cap);

    Streamer->setStreamingExposureEnabled(false);
    Streamer->setPixelFormat(INDI_JPG);

    addConfigurationControl();
    addDebugControl();
    return true;
}

void PentaxCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool PentaxCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        deleteProperty("CCD_COMPRESSION");
        setupParams();

        buildCaptureSwitches();

        defineProperty(&transferFormatSP);
        defineProperty(&autoFocusSP);
        if (transferFormatS[0].s == ISS_ON) {
            defineProperty(&preserveOriginalSP);
        }

        timerID = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteCaptureSwitches();

        deleteProperty(autoFocusSP.name);
        deleteProperty(transferFormatSP.name);
        deleteProperty(preserveOriginalSP.name);

        rmTimer(timerID);
    }

    return true;
}

void PentaxCCD::buildCaptureSwitches() {
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

void PentaxCCD::deleteCaptureSwitches() {
    if (mIsoSP.nsp > 0) deleteProperty(mIsoSP.name);
    if (mApertureSP.nsp > 0) deleteProperty(mApertureSP.name);
    if (mExpCompSP.nsp > 0) deleteProperty(mExpCompSP.name);
    if (mWhiteBalanceSP.nsp > 0) deleteProperty(mWhiteBalanceSP.name);
    if (mIQualitySP.nsp > 0) deleteProperty(mIQualitySP.name);
    if (mFormatSP.nsp > 0) deleteProperty(mFormatSP.name);
    if (mStorageWritingSP.nsp > 0) deleteProperty(mStorageWritingSP.name);
}

void PentaxCCD::refreshBatteryStatus() {
    char batterylevel[10];
    sprintf(batterylevel,"%d%%",device->getStatus().getBatteryLevel());
    IUSaveText(&DeviceInfoT[4],batterylevel);
    IDSetText(&DeviceInfoTP,nullptr);
}

bool PentaxCCD::Connect()
{
    if (device->getEventListeners().size() == 0) {
        listener = std::make_shared<PentaxEventHandler>(this);
        device->addEventListener(listener);
    }
    Response response = device->connect(DeviceInterface::USB);
    if (response.getResult() == Result::Ok) {
        LOG_INFO("Camera connected.");
    } else {
        LOG_INFO("Error connecting to camera.");
        for (const auto& error : response.getErrors()) {
            LOGF_INFO("Error Code: %d (%s)",static_cast<int>(error->getCode()),error->getMessage().c_str());
        }
        return false;
    }
    LOG_INFO("Connected to Pentax camera in PTP mode.");
    return true;
}

bool PentaxCCD::Disconnect()
{
    if (device->isConnected(DeviceInterface::USB)) {
        Response response = device->disconnect(DeviceInterface::USB);
        if (response.getResult() == Result::Ok) {
            LOG_INFO("Camera disconnected.");
        } else {
            LOG_INFO("Error disconnecting from camera.");
            for (const auto& error : response.getErrors()) {
                LOGF_INFO("Error Code: %d (%s)",static_cast<int>(error->getCode()),error->getMessage().c_str());
            }
            return false;
        }
    }
    return true;

}

bool PentaxCCD::setupParams()
{
    getCaptureSettingsState();
    return true;
}


bool PentaxCCD::StartExposure(float duration)
{
    if (InExposure)
    {
        LOG_ERROR("Camera is already exposing.");
        return false;
    }
    else if (Streamer->isBusy()) {
        LOG_WARN("Cannot start exposure because the camera is streaming.  Please stop streaming first.");
        return false;
    }
    else {

        InExposure = true;

        //update shutter speed if needed
        double ret = updateShutterSpeed(duration);
        if (ret) duration = ret;
        PrimaryCCD.setExposureDuration(duration);
        ExposureRequest = duration;

        //apply any outstanding capture settings changes
        if (updatedCaptureSettings.size()>0) {
            LOG_INFO("Updating camera capture settings.");
            Response response = device->setCaptureSettings(updatedCaptureSettings);
            if (response.getResult() == Result::Error) {
                for (const auto& error : response.getErrors()) {
                    LOGF_ERROR("Error setting updating capture settings (%d): %s", static_cast<int>(error->getCode()), error->getMessage().c_str());
                }
            }
            updatedCaptureSettings.clear();
            getCaptureSettingsState();
        }

        //start capture
        gettimeofday(&ExpStart, nullptr);
        LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);
        try
        {
            StartCaptureResponse response = device->startCapture((autoFocusS[0].s == ISS_ON));
            if (response.getResult() == Result::Ok) {
                pendingCapture = response.getCapture();
                LOGF_INFO("Capture has started. Capture ID: %s",response.getCapture()->getId().c_str());
                InDownload = false;
            } else {
                LOGF_ERROR("Capture failed to start (%s)",response.getErrors()[0]->getMessage().c_str());
                InExposure = false;
                return false;
            }
        }
        catch (const std::runtime_error &e){
            LOGF_ERROR("runtime_error: %s", e.what());
            InExposure = false;
            return false;
        }

        return true;
    }
}

bool PentaxCCD::AbortExposure()
{
    if (Streamer->isBusy()) {
        LOG_INFO("Camera is currently streaming.  Driver will abort without stopping camera.");
    }
    else {
        Response response = device->stopCapture();
        if (response.getResult() == Result::Ok) {
            LOG_INFO("Capture aborted.");
            InExposure = false;
        } else {
            LOGF_ERROR("Capture failed to abort (%s)",response.getErrors()[0]->getMessage().c_str());
            return false;
        }
    }
    return true;
}

bool PentaxCCD::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
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


float PentaxCCD::CalcTimeLeft()
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

void PentaxCCD::TimerHit()
{
    int timerID = -1;
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();
        if (pendingCapture->getState()==CaptureState::Complete) {
            InExposure = false;
            InDownload = true;
        }
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
                    InDownload = true;
                    LOG_INFO("Capture finished.  Waiting for image download...");
                    InExposure=false;
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

    if (InDownload) {
        if (pendingCapture->getState()==CaptureState::Complete) {
            if (bufferIsBayered) SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
            else SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
            InDownload = false;
            ExposureComplete(&PrimaryCCD);
        }
        else if (pendingCapture->getState()==CaptureState::Unknown) {
            LOG_ERROR("Capture entered unknown state.  Aborting...");
            AbortExposure();
        }
        else if (isDebug())
        {
            IDLog("Still waiting for download...");
        }
    }

    if (timerID == -1)
        SetTimer(getCurrentPollingPeriod());
    return;
}

bool PentaxCCD::StartStreaming()
{
    if (InExposure) {
        LOG_WARN("Camera is in the middle of an exposure.  Please wait until finished, or abort.");
        return false;
    }
    else if (Streamer->isBusy()) {
        LOG_WARN("Streamer is already active.");
        return false;
    }
    else {
        Response response = device->startLiveView();
        if (response.getResult() == Result::Ok) {
            LOG_INFO("Started streamer.");
            return true;
        } else {
            LOG_ERROR("Could not start streamer.");
            return false;
        }
    }
}

bool PentaxCCD::StopStreaming()
{
    Response response = device->stopLiveView();
    if (response.getResult() == Result::Ok) {
        LOG_INFO("Stopped streamer.");
        return true;
    } else {
        LOG_ERROR("Could not stop streamer.");
        return false;
    }
}


ISwitch * PentaxCCD::create_switch(const char * basestr, std::vector<string> options, int setidx)
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

float convertShutterSpeedString(string str) {
    float num = stof(str);
    int dividx = str.find("/");
    int denom = 1;
    if (dividx>-1) {
        denom = stoi(str.substr(dividx+1,str.length()-dividx));
    }
    return num/denom;
}

float PentaxCCD::updateShutterSpeed(float requestedSpeed) {

    double lowestdiff = 7200;
    const CaptureSetting *targetSpeed = nullptr;
    float targetSpeed_f;
    for (const auto ss : shutter.getAvailableSettings()) {
        float ss_f = convertShutterSpeedString(ss->getValue().toString());
        double currentdiff = abs(ss_f - requestedSpeed);
        if (currentdiff < lowestdiff) {
            targetSpeed = ss;
            targetSpeed_f = ss_f;
            lowestdiff = currentdiff;
        }
        else if (targetSpeed) {
            break;
        }
    }
    if (targetSpeed) {
        if (requestedSpeed != targetSpeed_f) {
            LOGF_INFO("Requested shutter speed of %f not supported.  Setting to closest supported speed: %f.",requestedSpeed,targetSpeed_f);
        }
        if (convertShutterSpeedString(shutter.getValue().toString()) != targetSpeed_f) {
            //updatedCaptureSettings.push_back(targetSpeed);
            updatedCaptureSettings.insert(updatedCaptureSettings.begin(),targetSpeed);
        }
        else {
            LOGF_DEBUG("Shutter speed already %f, not setting.",targetSpeed_f);
        }
        return targetSpeed_f;
    }
    LOG_INFO("The camera is currently in an exposure program that does not permit setting the shutter speed externally.  Shutter speed will instead be controlled by camera.");
    return requestedSpeed;
}

void PentaxCCD::updateCaptureSetting(CaptureSetting *setting, string newval) {
    for (auto i : setting->getAvailableSettings()) {
        if (i->getValue().toString() == newval) {
            updatedCaptureSettings.push_back(i);
            return;
        }
    }
    LOGF_ERROR("Error setting %S to %s: not supported in current camera mode", setting->getName().c_str(), newval.c_str());
}

bool PentaxCCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
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
            defineProperty(&preserveOriginalSP);
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
        updateCaptureSettingSwitch(&iso,&mIsoSP,states,names,n);
    }
    else if (!strcmp(name, mApertureSP.name)) {
        updateCaptureSettingSwitch(&aperture,&mApertureSP,states,names,n);
    }
    else if (!strcmp(name, mExpCompSP.name)) {
        updateCaptureSettingSwitch(&exposurecomp,&mExpCompSP,states,names,n);
    }
    else if (!strcmp(name, mWhiteBalanceSP.name)) {
        updateCaptureSettingSwitch(&whitebalance,&mWhiteBalanceSP,states,names,n);
    }
    else if (!strcmp(name, mIQualitySP.name)) {
        updateCaptureSettingSwitch(&imagequality,&mIQualitySP,states,names,n);
    }
    else if (!strcmp(name, mFormatSP.name)) {
        updateCaptureSettingSwitch(&imageformat,&mFormatSP,states,names,n);
    }
    else if (!strcmp(name, mStorageWritingSP.name)) {
        updateCaptureSettingSwitch(&storagewriting,&mStorageWritingSP,states,names,n);
    }
    else {
        return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
    }
    return true;
}

void PentaxCCD::updateCaptureSettingSwitch(CaptureSetting * setting, ISwitchVectorProperty * sw, ISState * states, char * names[], int n) {
    ISwitch * previous = IUFindOnSwitch(sw);
    IUUpdateSwitch(sw, states, names, n);
    sw->s = IPS_OK;
    IDSetSwitch(sw, nullptr);
    ISwitch * selected = IUFindOnSwitch(sw);
    if (previous != selected) updateCaptureSetting(setting,selected->label);
}

bool PentaxCCD::saveConfigItems(FILE * fp) {

    for (auto sw : std::vector<ISwitchVectorProperty*>{&mIsoSP,&mApertureSP,&mExpCompSP,&mWhiteBalanceSP,&mIQualitySP,&mFormatSP,&mStorageWritingSP}) {
        if (sw->nsp>0) IUSaveConfigSwitch(fp, sw);
    }

    // Save regular CCD properties
    return INDI::CCD::saveConfigItems(fp);
}

void PentaxCCD::addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip)
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

void PentaxCCD::buildCaptureSettingSwitch(ISwitchVectorProperty *control, CaptureSetting *setting, const char *label, const char *name) {
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
        defineProperty(control);
    }
}

void PentaxCCD::getCaptureSettingsState() {
    Response response = device->getCaptureSettings(std::vector<CaptureSetting*>{&iso,&shutter,&aperture,&exposurecomp,&whitebalance,&imagequality,&imageformat,&exposureprogram,&storagewriting,&usercapturesettingsmode});
    if (response.getResult() != Result::Ok) {
        for (const auto& error : response.getErrors()) {
            LOGF_ERROR("Error getting camera state (%d): %s", static_cast<int>(error->getCode()), error->getMessage().c_str());
        }
    }
}

string PentaxCCD::getUploadFilePrefix() {
    return UploadSettingsT[UPLOAD_DIR].text + string("/") + UploadSettingsT[UPLOAD_PREFIX].text;
}
