/*
 SV305 CCD
 SVBONY SV305 Camera driver
 Copyright (C) 2020 Blaise-Florentin Collin (thx8411@yahoo.fr)

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

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"

#include "../libsv305/CKCameraInterface.h"

#include "sv305_ccd.h"

#define MAX_DEVICES    8   /* Max device cameraCount */

static int cameraCount;
static Sv305CCD *cameras[MAX_DEVICES];

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
} deviceTypes[] = { { 0x0001, 0x0001, "SV305" }, { 0, 0, nullptr } };

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
     /**********************************************************
     *
     *  IMPORTANT: If available use CCD API function for enumeration available CCD's otherwise use code like this:
     *
     **********************************************************

     cameraCount = 0;
     for (struct usb_bus *bus = usb_get_busses(); bus && cameraCount < MAX_DEVICES; bus = bus->next) {
       for (struct usb_device *dev = bus->devices; dev && cameraCount < MAX_DEVICES; dev = dev->next) {
         int vid = dev->descriptor.idVendor;
         int pid = dev->descriptor.idProduct;
         for (int i = 0; deviceTypes[i].pid; i++) {
           if (vid == deviceTypes[i].vid && pid == deviceTypes[i].pid) {
             cameras[i] = new Sv305CCD(dev, deviceTypes[i].name);
             break;
           }
         }
       }
     }
     */

        /* For demo purposes we are creating two test devices */
        cameraCount            = 1;
        struct usb_device *dev = nullptr;
        cameras[0]             = new Sv305CCD(dev, deviceTypes[0].name);

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        Sv305CCD *camera = cameras[i];
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
        Sv305CCD *camera = cameras[i];
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
        Sv305CCD *camera = cameras[i];
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
        Sv305CCD *camera = cameras[i];
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
        Sv305CCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

Sv305CCD::Sv305CCD(DEVICE device, const char *name)
{
    this->device = device;
    snprintf(this->name, 32, "SVBONY SV305 CCD %s", name);
    setDeviceName(this->name);

    setVersion(SV305_VERSION_MAJOR, SV305_VERSION_MINOR);
}

Sv305CCD::~Sv305CCD()
{
}

const char *Sv305CCD::getDefaultName()
{
    return "SVBONY SV305 CCD";
}

bool Sv305CCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    SetCCDCapability(CCD_CAN_ABORT);

    addConfigurationControl();
    addDebugControl();
    return true;
}

void Sv305CCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool Sv305CCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        rmTimer(timerID);
    }

    return true;
}

bool Sv305CCD::Connect()
{
    LOG_INFO("Attempting to find the SVBONY SV305 CCD...");

    /**********************************************************
   *
   *
   *
   *  IMPORTANT: Put here your CCD Connect function
   *  If you encameraCounter an error, send the client a message
   *  e.g.
   *  LOG_INFO( "Error, unable to connect due to ...");
   *  return false;
   *
   *
   **********************************************************/

    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    return true;
}

bool Sv305CCD::Disconnect()
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

    LOG_INFO("CCD is offline.");
    return true;
}

bool Sv305CCD::setupParams()
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
    // 4. Get pixel depth
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

bool Sv305CCD::StartExposure(float duration)
{
    if (duration < minDuration)
    {
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,
               minDuration);
        duration = minDuration;
    }

    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD start exposure here
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

    gettimeofday(&ExpStart, nullptr);
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;

    return true;
}

bool Sv305CCD::AbortExposure()
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


float Sv305CCD::CalcTimeLeft()
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
int Sv305CCD::grabImage()
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

void Sv305CCD::TimerHit()
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
                    //  it's real close now, so spin on it
                    while (timeleft > 0)
                    {
                        /**********************************************************
             		*
             		*  IMPORRANT: If supported by your CCD API
             		*  Add a call here to check if the image is ready for download
             		*  If image is ready, set timeleft to 0. Some CCDs (check FLI)
             		*  also return timeleft in msec.
             		*
             		**********************************************************/

                        // Breaking in simulation, in real driver either loop until time left = 0 or use an API call to know if the image is ready for download
                        break;

                        //int slv;
                        //slv = 100000 * timeleft;
                        //usleep(slv);
                    }

                    /* We're done exposing */
                    LOG_INFO("Exposure done, downloading image...");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;
                    /* grab and save image */
                    grabImage();
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
