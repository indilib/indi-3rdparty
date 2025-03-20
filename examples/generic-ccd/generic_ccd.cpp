/*
 Generic CCD
 CCD Template for INDI Developers
 Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <memory>
#include <deque>

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"

#include "generic_ccd.h"

static struct
{
    int vid;
    int pid;
    const char *name;
} deviceTypes[] = { { 0x0001, 0x0001, "Model 1" }, { 0x0001, 0x0002, "Model 2" }, { 0, 0, nullptr } };

///////////////////////////////////////////////////////////////////////////////////////
/// Loader is what gets called first to query and create drivers for the attached cameras.
///////////////////////////////////////////////////////////////////////////////////////
static class Loader
{
        std::deque<std::unique_ptr<GenericCCD>> cameras;
    public:
        Loader()
        {
            /* For demo purposes we are creating two test devices */
            struct usb_device *dev = nullptr;
            cameras.push_back(std::unique_ptr<GenericCCD>(new GenericCCD(dev, deviceTypes[0].name)));
            cameras.push_back(std::unique_ptr<GenericCCD>(new GenericCCD(dev, deviceTypes[1].name)));
        }
} loader;

///////////////////////////////////////////////////////////////////////////////////////
/// Create a new generic camera
///////////////////////////////////////////////////////////////////////////////////////
GenericCCD::GenericCCD(DEVICE device, const char *name)
{
    this->device = device;
    snprintf(this->m_Name, MAXINDINAME, "Generic CCD %s", name);
    setDeviceName(this->m_Name);

    setVersion(GENERIC_VERSION_MAJOR, GENERIC_VERSION_MINOR);
}

///////////////////////////////////////////////////////////////////////////////////////
/// Destructor: Clean up any resources.
///////////////////////////////////////////////////////////////////////////////////////
GenericCCD::~GenericCCD()
{
}

///////////////////////////////////////////////////////////////////////////////////////
const char *GenericCCD::getDefaultName()
{
    return "Generic CCD";
}


///////////////////////////////////////////////////////////////////////////////////////
/// This function is called to initialize the driver properties for the first time.
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::initProperties()
{
    // Always call parent initProperties first.
    INDI::CCD::initProperties();

    // Next, let's setup which capabilities are offered by our camera?
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER | CCD_HAS_ST4_PORT;
    SetCCDCapability(cap);

    // Simulate Crash
    CrashSP.fill(getDeviceName(), "CCD_SIMULATE_CRASH", "Crash", OPTIONS_TAB, IP_WO, ISR_ATMOST1, 60, IPS_IDLE);
    CrashSP[0].fill("CRASH", "Crash driver", ISS_OFF);

    // Add configuration for Debug
    addDebugControl();

    // We're done!
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// updateProperties is called whenever Connect/Disconnect event is triggered.
/// If we are now connected to the camera, we might want to query some parameters.
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();
        CrashSP.define();
        SetTimer(getCurrentPollingPeriod());
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Establish connection to the camera
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::Connect()
{
    LOG_INFO("Attempting to find the Generic CCD...");

    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD Connect function
    *  If you encameraCounter an error, send the client a message
    *  e.g.
    *  LOG_INFO( "Error, unable to connect due to ...");
    *  return false;
    *
    *
    **********************************************************/

    /* Success! */
    LOG_INFO("Camera is online. Retrieving basic data.");

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Disconnect from the camera
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::Disconnect()
{
    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD disonnect function
    *  If you encameraCounter an error, send the client a message
    *  e.g.
    *  LOG_INFO( "Error, unable to disconnect due to ...");
    *  return false;
    *
    *
    **********************************************************/

    LOG_INFO("Camera is offline.");
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// This is called after connecting to the camera to setup some parameters.
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::setupParams()
{
    float x_pixel_size, y_pixel_size;
    int bit_depth = 16;
    int x_1, y_1, x_2, y_2;

    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Get basic CCD parameters here such as
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
    x_pixel_size = 5.4;
    y_pixel_size = 5.4;

    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////

    // Actucal CALL to CCD to get frame information here
    x_1 = y_1 = 0;
    x_2       = 1280;
    y_2       = 1024;

    ///////////////////////////
    // 3. Get temperature
    ///////////////////////////
    // Setting sample temperature -- MAKE CALL TO API FUNCTION TO GET TEMPERATURE IN REAL DRIVER
    TemperatureNP[0].setValue(25.0);
    LOGF_INFO("The CCD Temperature is %f", TemperatureNP[0].getValue());
    TemperatureNP.apply();

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

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Set camera temperature
///////////////////////////////////////////////////////////////////////////////////////
int GenericCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (std::abs(temperature - TemperatureNP[0].getValue()) < TEMP_THRESHOLD)
        return 1;

    /**********************************************************
     *
     *  IMPORRANT: Put here your CCD Set Temperature Function
     *  We return 0 if setting the temperature will take some time
     *  If the requested is the same as current temperature, or very
     *  close, we return 1 and INDI::CCD will mark the temperature status as OK
     *  If we return 0, INDI::CCD will mark the temperature status as BUSY
     **********************************************************/

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Start Exposure
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::StartExposure(float duration)
{
    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD start exposure here
    *  Please note that duration passed is in seconds.
    *  If there is an error, report it back to client
    *  e.g.
    *  LOG_ERROR( "Error, unable to start exposure due to ...");
    *  return false;
    *
    *
    **********************************************************/

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    m_ElapsedTimer.start();
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);
    InExposure = true;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Abort Exposure
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::AbortExposure()
{
    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD abort exposure here
    *  If there is an error, report it back to client
    *  e.g.
    *  LOG_INFO( "Error, unable to abort exposure due to ...");
    *  return false;
    *
    *
    **********************************************************/

    InExposure = false;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Update Camera Frame Type
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
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
            *  IMPORRANT: Put here your CCD Frame type here
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
            *  IMPORRANT: Put here your CCD Frame type here
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

///////////////////////////////////////////////////////////////////////////////////////
/// Update Camera Region of Interest ROI
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::UpdateCCDFrame(int x, int y, int w, int h)
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
    *  IMPORRANT: Put here your CCD Frame dimension call
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

///////////////////////////////////////////////////////////////////////////////////////
/// Update Camera Binning
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::UpdateCCDBin(int binx, int biny)
{
    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD Binning call
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

///////////////////////////////////////////////////////////////////////////////////////
/// Download the image from the camera. Create random image.
///////////////////////////////////////////////////////////////////////////////////////
int GenericCCD::downloadImage()
{
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
    int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    /**********************************************************
     *
     *
     *  IMPORRANT: Put here your CCD Get Image routine here
     *  use the image, width, and height variables above
     *  If there is an error, report it back to client
     *
     *
     **********************************************************/
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            image[i * width + j] = rand() % 255;

    LOG_INFO("Download complete.");

    ExposureComplete(&PrimaryCCD);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
/// TimerHit is the main loop of the driver where it gets called every 1 second
/// by default. Here you perform checks on any ongoing operations and perhaps query some
/// status updates like temperature.
///////////////////////////////////////////////////////////////////////////////////////
void GenericCCD::TimerHit()
{
    if (isConnected() == false)
        return;

    // Are we in exposure? Let's check if we're done!
    if (InExposure)
    {
        // Seconds elapsed
        double timeLeft = ExposureRequest - m_ElapsedTimer.elapsed() / 1000.0;
        if (timeLeft <= 0)
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");

            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            // Download Image
            downloadImage();
        }
        else
            PrimaryCCD.setExposureLeft(timeLeft);
    }

    // Are we performing temperature readout or regulation?
    switch (TemperatureNP.getState())
    {
        case IPS_IDLE:
        case IPS_OK:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Get temperature call here
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to get temp due to ...");
            *  return false;
            *
            *
            **********************************************************/
            break;

        case IPS_BUSY:
            /**********************************************************
            *
            *
            *
            *  IMPORRANT: Put here your CCD Get temperature call here
            *  If there is an error, report it back to client
            *  e.g.
            *  LOG_INFO( "Error, unable to get temp due to ...");
            *  return false;
            *
            *
            **********************************************************/
            TemperatureNP[0].setValue(TemperatureRequest);

            // If we're within threshold, let's make it BUSY ---> OK
            if (std::abs(TemperatureRequest - TemperatureNP[0].getValue()) <= TEMP_THRESHOLD)
                TemperatureNP.setState(IPS_OK);

            TemperatureNP.apply();
            break;

        case IPS_ALERT:
            break;
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Guide to the north
///////////////////////////////////////////////////////////////////////////////////////
IPState GenericCCD::GuideNorth(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
    *
    *
    *
    *  IMPORRANT: Put here your CCD Guide call
    *  Some CCD API support pulse guiding directly (i.e. without timers)
    *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
    *  will have to start a timer and then stop it after the 'ms' milliseconds
    *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
    *  available in INDI 3rd party repository
    *  If there is an error, report it back to client
    *  e.g.
    *  LOG_ERROR( "Error, unable to guide due ...");
    *  return IPS_ALERT;
    *
    *
    **********************************************************/

    return IPS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////
/// Guide to the south
///////////////////////////////////////////////////////////////////////////////////////
IPState GenericCCD::GuideSouth(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
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

///////////////////////////////////////////////////////////////////////////////////////
/// Guide to the east
///////////////////////////////////////////////////////////////////////////////////////
IPState GenericCCD::GuideEast(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
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

///////////////////////////////////////////////////////////////////////////////////////
/// Guide to the west
///////////////////////////////////////////////////////////////////////////////////////
IPState GenericCCD::GuideWest(uint32_t ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
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

///////////////////////////////////////////////////////////////////////////////////////
/// Crash simulation
///////////////////////////////////////////////////////////////////////////////////////
bool GenericCCD::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CrashSP.isNameMatch(name))
        {
            abort();
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}
