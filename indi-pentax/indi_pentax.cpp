/*
 Pentax CCD
 CCD Template for INDI Developers
 Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <libraw.h>

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "../indi-gphoto/gphoto_readimage.cpp"

#include "indi_pentax.h"

#define MAX_CCD_TEMP   45   /* Max CCD temperature */
#define MIN_CCD_TEMP   -55  /* Min CCD temperature */
#define MAX_X_BIN      16   /* Max Horizontal binning */
#define MAX_Y_BIN      16   /* Max Vertical binning */
#define MAX_PIXELS     4096 /* Max number of pixels in one dimension */
#define TEMP_THRESHOLD .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES    20   /* Max device cameraCount */


static int cameraCount;
static PentaxCCD *cameras[MAX_DEVICES];
static char *logdevicename = "Pentax Driver";


/**********************************************************
 *
 *  IMPORTANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static struct
{
    int vid;
    int pid;
    const char *name;
    int maxwidth;
    int maxheight;
    float pixelsize;
} deviceTypes[] = { { 0x25fb, 0x017c, "PENTAX K70", 6000, 4000, 3.88 },
                    { 0x25fb, 0x0130, "PENTAX K-1 Mark II", 7360, 4920, 4.86 },
                    { 0x25fb, 0, "PENTAX K-1", 7360, 4920, 4.86 },
                    { 0x25fb, 0, "PENTAX KP", 6016, 4000, 3.88 },
                    { 0x25fb, 0, "PENTAX 645Z", 8256, 6192, 5.32 },
                    { 0, 0, nullptr, 0, 0, 0 } };

static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {

        std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
        cameraCount = detectedCameraDevices.size();
        if (cameraCount > 0) {
            DEBUGDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "Pentax Camera driver using Ricoh Camera SDK");
            DEBUGDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "Please be sure the camera is on, connected, and in PTP mode!!!");
            DEBUGFDEVICE(logdevicename,INDI::Logger::DBG_SESSION, "%d Pentax camera(s) have been detected.",cameraCount);
            for (size_t i = 0; (i < cameraCount) && (i < MAX_DEVICES); i++) {
                std::shared_ptr<CameraDevice> camera = detectedCameraDevices[i];
                cameras[i] = new PentaxCCD(camera);
            }
        } else {
            DEBUGDEVICE(logdevicename,INDI::Logger::DBG_ERROR, "No supported Pentax cameras were found.  Perhaps the camera is not supported, or not powered up and connected in PTP mode?");
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        PentaxCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        PentaxCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        PentaxCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        PentaxCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        PentaxCCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

PentaxEventListener::PentaxEventListener(const char *devname, INDI::CCDChip *ccd) {
    deviceName = devname;
    parentCCD = ccd;
}

const char * PentaxEventListener::getDeviceName() {
    return deviceName;
}

void PentaxEventListener::imageStored(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const CameraImage>& image)
{
    lastImage = image;
}

void PentaxEventListener::liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender, const std::shared_ptr<const unsigned char>& liveViewFrame, uint64_t frameSize)
{
    lastLiveViewFrame = liveViewFrame;
    lastLiveViewFrameSize = frameSize;
    lastLiveViewFrameRendered = false;
    LOG_DEBUG("Hi");
}

PentaxCCD::PentaxCCD(std::shared_ptr<CameraDevice> camera)
{
    this->device = camera;
    snprintf(this->name, 32, "Pentax %s", camera->getModel().c_str());
    setDeviceName(this->name);

    setVersion(INDI_PENTAX_VERSION_MAJOR, INDI_PENTAX_VERSION_MINOR);
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

    IUFillSwitchVector(&mIsoSP, nullptr, 0, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);
    IUFillSwitchVector(&mFormatSP, nullptr, 0, getDeviceName(), "CAPTURE_FORMAT", "Capture Format", IMAGE_SETTINGS_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitchVector(&mExposurePresetSP, nullptr, 0, getDeviceName(), "CCD_EXPOSURE_PRESETS", "Presets",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&autoFocusS[0], "Set", "", ISS_OFF);
    IUFillSwitchVector(&autoFocusSP, autoFocusS, 1, getDeviceName(), "Auto Focus", "", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillSwitch(&transferFormatS[0], "FORMAT_FITS", "FITS", ISS_ON);
    IUFillSwitch(&transferFormatS[1], "FORMAT_NATIVE", "Native", ISS_OFF);
    IUFillSwitchVector(&transferFormatSP, transferFormatS, 2, getDeviceName(), "CCD_TRANSFER_FORMAT", "Transfer Format",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&livePreviewS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&livePreviewS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&livePreviewSP, livePreviewS, 2, getDeviceName(), "AUX_VIDEO_STREAM", "Preview",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SDCardImageS[SD_CARD_SAVE_IMAGE], "Save", "", ISS_ON);
    IUFillSwitch(&SDCardImageS[SD_CARD_DELETE_IMAGE], "Delete", "", ISS_OFF);
    IUFillSwitchVector(&SDCardImageSP, SDCardImageS, 2, getDeviceName(), "CCD_SD_CARD_ACTION", "SD Image",
                       IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

    IUSaveText(&BayerT[2], "RGGB");

    PrimaryCCD.getCCDInfo()->p = IP_RW;


    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_BAYER | CCD_HAS_SHUTTER | CCD_HAS_STREAMING;
    SetCCDCapability(cap);

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
        if (mExposurePresetSP.nsp > 0)
            defineSwitch(&mExposurePresetSP);
        if (mIsoSP.nsp > 0)
            defineSwitch(&mIsoSP);
        if (mFormatSP.nsp > 0)
            defineSwitch(&mFormatSP);

        defineSwitch(&livePreviewSP);
        defineSwitch(&transferFormatSP);
        defineSwitch(&autoFocusSP);

        defineSwitch(&SDCardImageSP);

        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        if (mExposurePresetSP.nsp > 0)
            deleteProperty(mExposurePresetSP.name);
        if (mIsoSP.nsp > 0)
            deleteProperty(mIsoSP.name);
        if (mFormatSP.nsp > 0)
            deleteProperty(mFormatSP.name);

        deleteProperty(livePreviewSP.name);
        deleteProperty(autoFocusSP.name);
        deleteProperty(transferFormatSP.name);

        deleteProperty(SDCardImageSP.name);

        rmTimer(timerID);
    }

    return true;
}

bool PentaxCCD::Connect()
{
    LOG_INFO("Attempting to connect to the Pentax CCD...");

    LOGF_INFO("    Manufacturer    : %s",device->getManufacturer().c_str());
    LOGF_INFO("    Model           : %s",device->getModel().c_str());
    LOGF_INFO("    Firmware Version: %s",device->getFirmwareVersion().c_str());
    LOGF_INFO("    Serial Number   : %s",device->getSerialNumber().c_str());

    if (device->getEventListeners().size() == 0) {
        listener = std::make_shared<PentaxEventListener>(getDeviceName(),&PrimaryCCD);
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
    float x_pixel_size, y_pixel_size;
    int bit_depth = 16;
    int x_1, y_1, x_2, y_2;

    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Get basic CCD parameters here such as
   *  + Pixel Size X
   *  + Pixel Size Y
   *  + Bit Depth?
   *  + X, Y, W, H of frame
   *  + Temperature
   *  + ...etc
   *
   *
   *
   **********************************************************/

    ///////////////////////////
    // 1. Get Pixel size
    ///////////////////////////
    // Actucal CALL to CCD to get pixel size here
    x_pixel_size = 3.89;
    y_pixel_size = 3.89;

    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////

    // Actucal CALL to CCD to get frame information here
    x_1 = y_1 = 0;
    x_2       = 6000;
    y_2       = 4000;

    ///////////////////////////
    // 3. Get temperature
    ///////////////////////////
    // Setting sample temperature -- MAKE CALL TO API FUNCTION TO GET TEMPERATURE IN REAL DRIVER
    //TemperatureN[0].value = 25.0;
    //LOGF_INFO("The CCD Temperature is %f", TemperatureN[0].value);
    //IDSetNumber(&TemperatureNP, nullptr);

    ///////////////////////////
    // 4. Get temperature
    ///////////////////////////
    bit_depth = 16;
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Now we usually do the following in the hardware
    // Set Frame to LIGHT or NORMAL
    // Set Binning to 1x1
    /* Default frame type is NORMAL */

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8; //  this is pixel cameraCount
    nbuf += 512;                                                                  //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    //Streamer->setSize(720, 480);
    return true;
}


bool PentaxCCD::StartExposure(float duration)
{
    if (duration < minDuration)
    {
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,
               minDuration);
        duration = minDuration;
    }

    if (imageFrameType == INDI::CCDChip::BIAS_FRAME)
    {
        duration = minDuration;
        LOGF_INFO("Bias Frame (s) : %g\n", minDuration);
    }

    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD start exposure here
   *  Please note that duration passed is in seconds.
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to start exposure due to ...");
   *  return -1;
   *
   *
   **********************************************************/

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    device->setCaptureSettings(std::vector<const CaptureSetting*>{ShutterSpeed::SS60});

    gettimeofday(&ExpStart, nullptr);
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;

    /*char cmd[MAXRBUF]={0}, line[256]={0};
    snprintf(cmd, MAXRBUF, "pktriggercord-cli -t %g -o /home/krees/ptest1.jpg", ExposureRequest);

    FILE *handle = popen(cmd, "r");
    if (handle == nullptr) {
        LOG_DEBUG("Failed to run pktriggercord");
    }
    else {
        while (fgets(line, sizeof(line), handle) != nullptr) {
                LOGF_DEBUG("%s", line);
        }
    }*/

    try
    {
        StartCaptureResponse response = device->startCapture();
        if (response.getResult() == Result::Ok) {
            pendingCapture = response.getCapture();
            LOGF_INFO("Capture has started. Capture ID: %s",response.getCapture()->getId().c_str());
        } else {
            LOGF_INFO("Capture failed to start (%s)",response.getErrors()[0]->getMessage().c_str());
        }
    }
    catch (std::runtime_error e){
        LOGF_INFO("runtime_error: %s",e.what());
    }


    return true;
}

bool PentaxCCD::AbortExposure()
{
    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD abort exposure here
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to abort exposure due to ...");
   *  return false;
   *
   *
   **********************************************************/

    Response response = device->stopCapture();
    if (response.getResult() == Result::Ok) {
        LOG_INFO("Capture aborted.");
        InExposure = false;
    } else {
        LOGF_INFO("Capture failed to abort (%s)",response.getErrors()[0]->getMessage().c_str());
        return false;
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

bool PentaxCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long bin_width  = x_1 + (w / PrimaryCCD.getBinX());
    long bin_height = y_1 + (h / PrimaryCCD.getBinY());

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD Frame dimension call
   *  The values calculated above are BINNED width and height
   *  which is what most CCD APIs require, but in case your
   *  CCD API implementation is different, don't forget to change
   *  the above calculations.
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to set frame to ...");
   *  return false;
   *
   *
   **********************************************************/

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, w, h);

    int nbuf;
    nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8); //  this is pixel count
    nbuf += 512;                                               //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);

    return true;
}

bool PentaxCCD::UpdateCCDBin(int binx, int biny)
{
    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD Binning call
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to set binning to ...");
   *  return false;
   *
   *
   **********************************************************/

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
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

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int PentaxCCD::grabImage(std::shared_ptr<const CameraImage> image)
{
    uint8_t * memptr = PrimaryCCD.getFrameBuffer();
    size_t memsize = 0;
    int naxis = 2, w = 0, h = 0, bpp = 8;

    if (isSimulation())
    {
        uint16_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
        uint16_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        subW -= subW % 2;
        subH -= subH % 2;

        uint32_t size = subW * subH;

        if (PrimaryCCD.getFrameBufferSize() < static_cast<int>(size))
        {
            PrimaryCCD.setFrameBufferSize(size);
            memptr = PrimaryCCD.getFrameBuffer();
        }

        if (PrimaryCCD.getBPP() == 8)
        {
            for (uint32_t i = 0 ; i < size; i++)
                memptr[i] = rand() % 255;
        }
        else
        {
            uint16_t *buffer = reinterpret_cast<uint16_t*>(memptr);
            for (uint32_t i = 0 ; i < size; i++)
                buffer[i] = rand() % 65535;
        }

        PrimaryCCD.setFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), subW, subH);
        ExposureComplete(&PrimaryCCD);
        return true;
    }


    //write image to file
    std::ofstream o;
    char filename[32] = "/tmp/indi_pentax_";
    strcat(filename,image->getName().c_str());
    o.open(filename, std::ofstream::out | std::ofstream::binary);
    Response response = image->getData(o);
    o.close();
    if (response.getResult() == Result::Ok) {
        DEBUGFDEVICE(logdevicename,INDI::Logger::DBG_DEBUG, "Temp Image path: %s",filename);
    } else {
        for (const auto& error : response.getErrors()) {
            DEBUGFDEVICE(logdevicename,INDI::Logger::DBG_ERROR, "Error Code: %d (%s)", static_cast<int>(error->getCode()), error->getMessage().c_str());
        }
        return false;
    }


    //convert it for image buffer
    if (read_jpeg(filename, &memptr, &memsize, &naxis, &w, &h))
    {
        LOG_ERROR("Exposure failed to parse jpeg.");
        return false;
    }
    LOGF_DEBUG("read_jpeg: memsize (%d) naxis (%d) w (%d) h (%d) bpp (%d)", memsize, naxis, w, h, bpp);

    SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    PrimaryCCD.setImageExtension("fits");

    uint16_t subW = PrimaryCCD.getSubW();
    uint16_t subH = PrimaryCCD.getSubH();

    if (PrimaryCCD.getSubW() != 0 && (w > PrimaryCCD.getSubW() || h > PrimaryCCD.getSubH()))
        LOGF_WARN("Camera image size (%dx%d) is less than requested size (%d,%d). Purge configuration and update frame size to match camera size.", w, h, PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

    PrimaryCCD.setFrame(0, 0, w, h);
    PrimaryCCD.setFrameBuffer(memptr);
    PrimaryCCD.setFrameBufferSize(memsize, false);
    PrimaryCCD.setResolution(w, h);
    PrimaryCCD.setNAxis(naxis);
    PrimaryCCD.setBPP(bpp);

    ExposureComplete(&PrimaryCCD);

    //remove image file
    //std::remove(filename);


    LOG_INFO("Copied to frame buffer.");

    return true;
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
                    if (pendingCapture->getState()==CaptureState::Complete) {
                        /* We're done exposing */
                        LOG_INFO("Done downloading.  Now copying to frame buffer...");
                        timeleft=0;
                        PrimaryCCD.setExposureLeft(0);
                        InExposure = false;
                        /* grab and save image */
                        grabImage(listener->lastImage);
                    }
                    else if (pendingCapture->getState()==CaptureState::Unknown) {
                        LOG_ERROR("Capture entered unknown state.  Aborting...");
                        AbortExposure();
                    }

                    LOG_INFO("Waiting for download...");
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

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

IPState PentaxCCD::GuideNorth(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'ms' milliseconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to guide due ...");
   *  return IPS_ALERT;
   *
   *
   **********************************************************/

    return IPS_OK;
}

IPState PentaxCCD::GuideSouth(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORTANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}

IPState PentaxCCD::GuideEast(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORTANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}

IPState PentaxCCD::GuideWest(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORTANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}


bool PentaxCCD::StartStreaming()
{
    //Streamer->setPixelFormat(INDI_RGB);
    /*std::unique_lock<std::mutex> guard(liveStreamMutex);
    m_RunLiveStream = true;
    guard.unlock();
    liveViewThread = std::thread(&PentaxCCD::streamLiveView, this);

    return true;*/
}

bool PentaxCCD::StopStreaming()
{
    /*std::unique_lock<std::mutex> guard(liveStreamMutex);
    m_RunLiveStream = false;
    guard.unlock();
    liveViewThread.join();
    return true;*/
}

void PentaxCCD::streamLiveView()
{




/*    const char * previewData = nullptr;
    unsigned long int previewSize = 0;
    CameraFile * previewFile = nullptr;

    int rc = gp_file_new(&previewFile);
    if (rc != GP_OK)
    {
        LOGF_ERROR("Error creating gphoto file: %s", gp_result_as_string(rc));
        return;
    }

    char errMsg[MAXRBUF] = {0};
    while (true)
    {
        std::unique_lock<std::mutex> guard(liveStreamMutex);
        if (m_RunLiveStream == false)
            break;
        guard.unlock();





        rc = gphoto_capture_preview(gphotodrv, previewFile, errMsg);
        if (rc != GP_OK)
        {
            //LOGF_DEBUG("%s", errMsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (rc >= GP_OK)
        {
            rc = gp_file_get_data_and_size(previewFile, &previewData, &previewSize);
            if (rc != GP_OK)
            {
                LOGF_ERROR("Error getting preview image data and size: %s", gp_result_as_string(rc));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        uint8_t * inBuffer = reinterpret_cast<uint8_t *>(const_cast<char *>(previewData));

        //        if (streamSubframeS[1].s == ISS_ON)
        //        {
        //            if (liveVideoWidth <= 0)
        //            {
        //                read_jpeg_size(inBuffer, previewSize, &liveVideoWidth, &liveVideoHeight);
        //                Streamer->setSize(liveVideoWidth, liveVideoHeight);
        //            }

        //            std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        //            Streamer->newFrame(inBuffer, previewSize);
        //            ccdguard.unlock();
        //            continue;
        //        }

        uint8_t * ccdBuffer      = PrimaryCCD.getFrameBuffer();
        size_t size             = 0;
        int w = 0, h = 0, naxis = 0;

        // Read jpeg from memory
        std::unique_lock<std::mutex> ccdguard(ccdBufferLock);
        rc = read_jpeg_mem(inBuffer, previewSize, &ccdBuffer, &size, &naxis, &w, &h);

        if (rc != 0)
        {
            LOG_ERROR("Error getting live video frame.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (liveVideoWidth <= 0)
        {
            liveVideoWidth = w;
            liveVideoHeight = h;
            Streamer->setSize(liveVideoWidth, liveVideoHeight);
        }

        PrimaryCCD.setFrameBuffer(ccdBuffer);

        // We are done with writing to CCD buffer
        ccdguard.unlock();

        if (naxis != PrimaryCCD.getNAxis())
        {
            if (naxis == 1)
                Streamer->setPixelFormat(INDI_MONO);

            PrimaryCCD.setNAxis(naxis);
        }

        if (PrimaryCCD.getSubW() != w || PrimaryCCD.getSubH() != h)
        {
            Streamer->setSize(w, h);
            PrimaryCCD.setFrame(0, 0, w, h);
        }

        if (PrimaryCCD.getFrameBufferSize() != static_cast<int>(size))
            PrimaryCCD.setFrameBufferSize(size, false);

        Streamer->newFrame(ccdBuffer, size);
    }

    gp_file_unref(previewFile);*/
}
