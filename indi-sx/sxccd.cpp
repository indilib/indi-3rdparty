/*
 Starlight Xpress CCD INDI Driver

 Copyright (c) 2012-2013 Cloudmakers, s. r. o.
 All Rights Reserved.

 Bayer Support Added by Karl Rees, Copyright(c) 2019

 Code is based on SX INDI Driver by Gerry Rozema and Jasem Mutlaq
 Copyright(c) 2010 Gerry Rozema.
 Copyright(c) 2012 Jasem Mutlaq.
 All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include "sxccd.h"

#include "sxconfig.h"

#include <cmath>

#include <unistd.h>

#define SX_GUIDE_EAST  0x08 /* RA+ */
#define SX_GUIDE_NORTH 0x04 /* DEC+ */
#define SX_GUIDE_SOUTH 0x02 /* DEC- */
#define SX_GUIDE_WEST  0x01 /* RA- */
#define SX_CLEAR_NS    0x09
#define SX_CLEAR_WE    0x06

static int count = 0;
static SXCCD *cameras[20];

#define TIMER 1000

static void cleanup()
{
    for (int i = 0; i < count; i++)
    {
        delete cameras[i];
    }
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        DEVICE devices[20];
        const char *names[20];
        count = sxList(devices, names, 20);
        for (int i = 0; i < count; i++)
        {
            cameras[i] = new SXCCD(devices[i], names[i]);
        }
        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();
    for (int i = 0; i < count; i++)
    {
        SXCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(camera->name);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < count; i++)
    {
        SXCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(camera->name, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < count; i++)
    {
        SXCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewText(camera->name, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < count; i++)
    {
        SXCCD *camera = cameras[i];
        if (dev == nullptr || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(camera->name, name, values, names, num);
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

    for (int i = 0; i < count; i++)
    {
        SXCCD *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

void ExposureTimerCallback(void *p)
{
    ((SXCCD *)p)->ExposureTimerHit();
}

void GuideExposureTimerCallback(void *p)
{
    ((SXCCD *)p)->GuideExposureTimerHit();
}

void WEGuiderTimerCallback(void *p)
{
    ((SXCCD *)p)->WEGuiderTimerHit();
}

void NSGuiderTimerCallback(void *p)
{
    ((SXCCD *)p)->NSGuiderTimerHit();
}

SXCCD::SXCCD(DEVICE device, const char *name)
{
    this->device          = device;
    handle                = nullptr;
    model                 = 0;
    oddBuf                = nullptr;
    evenBuf               = nullptr;
    GuideStatus           = 0;
    TemperatureRequest    = 0;
    TemperatureReported   = 0;
    ExposureTimeLeft      = 0.0;
    GuideExposureTimeLeft = 0.0;
    HasShutter            = false;
    HasCooler             = false;
    HasST4Port            = false;
    HasGuideHead          = false;
    HasColor              = false;
    ExposureTimerID       = 0;
    DidFlush              = false;
    DidLatch              = false;
    GuideExposureTimerID  = 0;
    InGuideExposure       = false;
    DidGuideLatch         = false;
    NSGuiderTimerID       = 0;
    WEGuiderTimerID       = 0;
    snprintf(this->name, 32, "SX CCD %s", name);
    setDeviceName(this->name);
    setVersion(VERSION_MAJOR, VERSION_MINOR);
}

SXCCD::~SXCCD()
{
    if (handle)
        sxClose(&handle);
}

void SXCCD::debugTriggered(bool enable)
{
    sxDebug(enable);
}

void SXCCD::simulationTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

const char *SXCCD::getDefaultName()
{
    return "SX CCD";
}

bool SXCCD::initProperties()
{
    INDI::CCD::initProperties();

    addDebugControl();

    IUFillSwitch(&CoolerS[0], "COOLER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       60, IPS_IDLE);
    IUFillSwitch(&ShutterS[0], "SHUTTER_ON", "Manual open", ISS_OFF);
    IUFillSwitch(&ShutterS[1], "SHUTTER_OFF", "Manual close", ISS_ON);
    IUFillSwitchVector(&ShutterSP, ShutterS, 2, getDeviceName(), "CCD_SHUTTER", "Shutter", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    //Adding switch to let user indicate whether the CCD has a Bayer filter, since I do not know which models beyond UltraStar C actually do
    //    IUFillSwitch(&BayerS[0], "BAYER_TRUE", "True", ISS_OFF);
    //    IUFillSwitch(&BayerS[1], "BAYER_FALSE", "False", ISS_ON);
    //    IUFillSwitchVector(&BayerSP, BayerS, 2, getDeviceName(), "CCD_BAYER_FILTER", "Bayer Filter", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY,
    //                       60, IPS_IDLE);
    IUSaveText(&BayerT[2], "RGGB");

    //  we can expose less than 0.01 seconds at a time
    //  and we need to for an allsky in daytime
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.0001, 3600, 0.0001, false);

    return true;
}

bool SXCCD::updateProperties()
{
    INDI::CCD::updateProperties();
    if (isConnected())
    {
        SetupParms();
        if (HasCooler)
            defineSwitch(&CoolerSP);
        if (HasShutter)
            defineSwitch(&ShutterSP);
        //        if (HasColor) {
        //            defineSwitch(&BayerSP);
        //        }
    }
    else
    {
        if (HasCooler)
            deleteProperty(CoolerSP.name);
        if (HasShutter)
            deleteProperty(ShutterSP.name);
        //        if (HasColor) {
        //            deleteProperty(BayerSP.name);
        //        }
    }
    return true;
}

bool SXCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x / PrimaryCCD.getBinX();
    long y_1 = y / PrimaryCCD.getBinY();

    long x_2 = x_1 + (w / PrimaryCCD.getBinX());
    long y_2 = y_1 + (h / PrimaryCCD.getBinY());

    if (x_2 > PrimaryCCD.getXRes())
    {
        LOGF_ERROR("Error: Requested image out of bounds (%ld, %ld)", x_2, y_2);
        return false;
    }
    else if (y_2 > PrimaryCCD.getYRes())
    {
        LOGF_ERROR("Error: Requested image out of bounds (%ld, %ld)", x_2, y_2);
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    return true;
}

bool SXCCD::UpdateCCDBin(int hor, int ver)
{
    if (hor == 3 || ver == 3)
    {
        IDMessage(getDeviceName(), "3x3 binning is not supported.");
        return false;
    }
    if (sxIsICX453(model) && hor != ver)
    {
        IDMessage(getDeviceName(), "Asymetric binning is not supported.");
        return false;
    }
    PrimaryCCD.setBin(hor, ver);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

bool SXCCD::Connect()
{
    if (handle == nullptr)
    {
        int rc = sxOpen(device, &handle);
        if (rc >= 0)
        {
            struct t_sxccd_params params;
            model = sxGetCameraModel(handle);
            LOGF_DEBUG("Camera model: %u", model);
            sxGetCameraParams(handle, 0, &params);

            HasGuideHead = params.extra_caps & SXCCD_CAPS_GUIDER;
            LOGF_DEBUG("Camera guide head: %s", HasGuideHead ? "Yes" : "No");

            HasCooler    = params.extra_caps & SXUSB_CAPS_COOLER;
            LOGF_DEBUG("Camera cooler: %s", HasCooler ? "Yes" : "No");

            HasShutter   = params.extra_caps & SXUSB_CAPS_SHUTTER;
            LOGF_DEBUG("Camera shutter: %s", HasShutter ? "Yes" : "No");

            HasST4Port   = params.extra_caps & SXCCD_CAPS_STAR2K;
            LOGF_DEBUG("Camera ST4 Port: %s", HasGuideHead ? "Yes" : "No");

            HasColor     = sxIsColor(model);
            LOGF_DEBUG("Camera color: %s", HasGuideHead ? "Yes" : "No");

            uint32_t cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME | CCD_CAN_BIN;

            if (HasColor)
                cap |= CCD_HAS_BAYER;

            if (HasCooler)
                cap |= CCD_HAS_COOLER;

            if (HasGuideHead)
                cap |= CCD_HAS_GUIDE_HEAD;

            if (HasShutter)
                cap |= CCD_HAS_SHUTTER;

            if (HasST4Port)
                cap |= CCD_HAS_ST4_PORT;

            SetCCDCapability(cap);

            return true;
        }
    }
    return false;
}

bool SXCCD::Disconnect()
{
    if (handle != nullptr)
    {
        sxClose(&handle);
    }
    return true;
}

void SXCCD::SetupParms()
{
    struct t_sxccd_params params;
    model             = sxGetCameraModel(handle);
    bool isInterlaced = sxIsInterlaced(model);
    bool isICX453     = sxIsICX453(model);
    sxGetCameraParams(handle, 0, &params);
    if (isInterlaced)
    {
        params.pix_height /= 2;
        params.height *= 2;
        wipeDelay = 130000;
    }
    else if (isICX453)
    {
        params.width = 3032;
        params.height = 2016;
    }
    SetCCDParams(params.width, params.height, params.bits_per_pixel, params.pix_width, params.pix_height);

    int nbuf = params.width * params.height;
    if (params.bits_per_pixel == 16)
        nbuf *= 2;
    //nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);
    if (isInterlaced)
    {
        if (evenBuf != nullptr)
            delete evenBuf;
        if (oddBuf != nullptr)
            delete oddBuf;
        evenBuf      = new char[nbuf / 2];
        oddBuf       = new char[nbuf / 2];
    }
    else if (isICX453)
    {
        if (evenBuf != nullptr)
            delete evenBuf;
        evenBuf      = new char[nbuf];
    }

    if (HasGuideHead)
    {
        sxGetCameraParams(handle, 1, &params);
        nbuf = params.width * params.height;
        nbuf += 512;
        GuideCCD.setFrameBufferSize(nbuf);
        SetGuiderParams(params.width, params.height, params.bits_per_pixel, params.pix_width, params.pix_height);
    }

    SetTimer(TIMER);
}

void SXCCD::TimerHit()
{
    if (isConnected() && HasCooler)
    {
        if (!DidLatch && !DidGuideLatch)
        {
            unsigned char status;
            unsigned short temperature;
            sxSetCooler(handle, (unsigned char)(CoolerS[0].s == ISS_ON),
                        (unsigned short)(TemperatureRequest * 10 + 2730), &status, &temperature);
            TemperatureN[0].value = (temperature - 2730) / 10.0;
            if (TemperatureReported != TemperatureN[0].value)
            {
                TemperatureReported = TemperatureN[0].value;
                if (std::fabs(TemperatureRequest - TemperatureReported) < 1)
                    TemperatureNP.s = IPS_OK;
                else
                    TemperatureNP.s = IPS_BUSY;
                IDSetNumber(&TemperatureNP, nullptr);
            }
        }
    }
    if (InExposure && ExposureTimeLeft >= 0)
        PrimaryCCD.setExposureLeft(ExposureTimeLeft--);
    if (InGuideExposure && GuideExposureTimeLeft >= 0)
        GuideCCD.setExposureLeft(GuideExposureTimeLeft--);
    if (isConnected())
        SetTimer(TIMER);
}

int SXCCD::SetTemperature(double temperature)
{
    int result         = 0;
    TemperatureRequest = temperature;
    unsigned char status;
    unsigned short sx_temperature;
    sxSetCooler(handle, (unsigned char)(CoolerS[0].s == ISS_ON), (unsigned short)(TemperatureRequest * 10 + 2730),
                &status, &sx_temperature);
    TemperatureReported = TemperatureN[0].value = (sx_temperature - 2730) / 10.0;
    if (std::fabs(TemperatureRequest - TemperatureReported) < 1)
        result = 1;
    else
        result = 0;

    CoolerSP.s   = IPS_OK;
    CoolerS[0].s = ISS_ON;
    CoolerS[1].s = ISS_OFF;
    IDSetSwitch(&CoolerSP, nullptr);

    return result;
}

bool SXCCD::StartExposure(float n)
{
    InExposure = true;
    PrimaryCCD.setExposureDuration(n);
    if (sxIsInterlaced(model) && PrimaryCCD.getBinY() == 1)
    {
        sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_EVEN | CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
        usleep(wipeDelay);
        sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_ODD | CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
    }
    else
        sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0);
    if (HasShutter && PrimaryCCD.getFrameType() != INDI::CCDChip::DARK_FRAME)
        sxSetShutter(handle, 0);
    int time = (int)(1000 * n);
    if (time < 1)
        time = 1;
    if (time > 3000)
    {
        DidFlush = false;
        time -= 3000;
    }
    else
        DidFlush = true;
    DidLatch         = false;
    ExposureTimeLeft = n;
    ExposureTimerID  = IEAddTimer(time, ExposureTimerCallback, this);
    return true;
}

bool SXCCD::AbortExposure()
{
    if (InExposure)
    {
        if (ExposureTimerID)
            IERmTimer(ExposureTimerID);
        if (HasShutter)
            sxSetShutter(handle, 1);
        ExposureTimerID = 0;
        PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
        DidLatch = false;
        DidFlush = false;
        return true;
    }
    return false;
}

void SXCCD::ExposureTimerHit()
{
    if (InExposure)
    {
        if (!DidFlush)
        {
            ExposureTimerID = IEAddTimer(3000, ExposureTimerCallback, this);
            sxClearPixels(handle, CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
            DidFlush = true;
        }
        else
        {
            int rc;
            ExposureTimerID   = 0;
            bool isInterlaced = sxIsInterlaced(model);
            int subX          = PrimaryCCD.getSubX();
            int subY          = PrimaryCCD.getSubY();
            int subW          = PrimaryCCD.getSubW();
            int subH          = PrimaryCCD.getSubH();
            int binX          = PrimaryCCD.getBinX();
            int binY          = PrimaryCCD.getBinY();
            int subWW         = subW * 2;
            bool isICX453     = sxIsICX453(model);
            uint8_t *buf      = PrimaryCCD.getFrameBuffer();
            int size;
            if (isInterlaced && binY > 1)
                size = subW * subH / 2 / binX / (binY / 2);
            else
                size = subW * subH / binX / binY;
            if (HasShutter)
                sxSetShutter(handle, 1);
            DidLatch = true;
            if (isInterlaced)
            {
                if (binY > 1)
                {
                    rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0, subX, subY / binY, subW, subH / 2, binX,
                                       binY / 2);
                    if (rc)
                        rc = sxReadPixels(handle, buf, size * 2);
                }
                else
                {
                    rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_EVEN | CCD_EXP_FLAGS_SPARE2, 0, subX, subY / 2, subW,
                                       subH / 2, binX, 1);
                    struct timeval tv;
                    gettimeofday(&tv, nullptr);
                    long startTime = tv.tv_sec * 1000000 + tv.tv_usec;
                    if (rc)
                        rc = sxReadPixels(handle, evenBuf, size);
                    gettimeofday(&tv, nullptr);
                    wipeDelay = tv.tv_sec * 1000000 + tv.tv_usec - startTime;
                    if (rc)
                        rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_ODD | CCD_EXP_FLAGS_SPARE2, 0, subX, subY / 2,
                                           subW, subH / 2, binX, 1);
                    if (rc)
                        rc = sxReadPixels(handle, oddBuf, size);
                    if (rc)
                    {
                        for (int i = 0, j = 0; i < subH; i += 2, j++)
                        {
                            memcpy(buf + i * subWW, oddBuf + (j * subWW), subWW);
                            memcpy(buf + ((i + 1) * subWW), evenBuf + (j * subWW), subWW);
                        }
                        //            deinterlace((unsigned short *)buf, subW, subH);
                    }
                }
            }
            else if (isICX453)
            {
                rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0, subX * 2, subY / 2, subW * 2, subH / 2, binX, binY);
                if (rc)
                {
                    if (binX == 1 && binY == 1)
                    {
                        rc = sxReadPixels(handle, evenBuf, size * 2);
                        if (rc)
                        {
                            uint16_t *buf16 = reinterpret_cast<uint16_t *>(buf);
                            uint16_t *evenBuf16 = reinterpret_cast<uint16_t *>(evenBuf);

                            int offset_1 = 2, offset_2 = 3;
                            if (strstr(getDeviceName(), "SXVF-M25C"))
                            {
                                // Patch by Greg Bosch on 2020-01-02 to fix bayer pattern
                                // on SXVF-M25C.
                                offset_1 = 3;
                                offset_2 = 2;
                            }

                            for (int i = 0; i < subH; i += 2)
                            {
                                for (int j = 0; j < subW; j += 2)
                                {
                                    int isubW = i * subW;
                                    int i1subW = (i + 1) * subW;
                                    int j2 = j * 2;

                                    buf16[isubW + j]  = evenBuf16[isubW + j2];
                                    buf16[isubW + j + 1]  = evenBuf16[isubW + j2 + offset_1];
                                    buf16[i1subW + j]  = evenBuf16[isubW + j2 + 1];
                                    buf16[i1subW + j + 1]  = evenBuf16[isubW + j2 + offset_2];

                                }
                            }
                        }
                    }
                    else
                    {
                        rc = sxReadPixels(handle, buf, size * 2);
                    }
                }
            }
            else
            {
                rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0, subX, subY, subW, subH, binX, binY);
                if (rc)
                    rc = sxReadPixels(handle, buf, size * 2);
            }
            DidLatch   = false;
            InExposure = false;
            PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
            if (rc)
                ExposureComplete(&PrimaryCCD);
        }
    }
}

bool SXCCD::StartGuideExposure(float n)
{
    InGuideExposure = true;
    GuideCCD.setExposureDuration(n);
    sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 1);
    int time = (int)(1000 * n);
    if (time < 1)
        time = 1;
    ExposureTimeLeft     = n;
    GuideExposureTimerID = IEAddTimer(time, GuideExposureTimerCallback, this);
    return true;
}

bool SXCCD::AbortGuideExposure()
{
    if (InGuideExposure)
    {
        if (GuideExposureTimerID)
            IERmTimer(GuideExposureTimerID);
        GuideCCD.setExposureLeft(GuideExposureTimeLeft = 0);
        GuideExposureTimerID = 0;
        DidGuideLatch        = false;
        return true;
    }
    return false;
}

void SXCCD::GuideExposureTimerHit()
{
    if (InGuideExposure)
    {
        int rc;
        GuideExposureTimerID = 0;
        int subX             = GuideCCD.getSubX();
        int subY             = GuideCCD.getSubY();
        int subW             = GuideCCD.getSubW();
        int subH             = GuideCCD.getSubH();
        int binX             = GuideCCD.getBinX();
        int binY             = GuideCCD.getBinY();
        int size             = subW * subH / binX / binY;
        uint8_t *buf         = GuideCCD.getFrameBuffer();
        DidGuideLatch        = true;
        rc                   = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 1, subX, subY, subW, subH, binX, binY);
        if (rc)
            rc = sxReadPixels(handle, buf, size);
        DidGuideLatch   = false;
        InGuideExposure = false;
        GuideCCD.setExposureLeft(GuideExposureTimeLeft = 0);
        if (rc)
            ExposureComplete(&GuideCCD);
    }
}

IPState SXCCD::GuideWest(uint32_t ms)
{
    if (!HasST4Port || ms < 1)
    {
        return IPS_ALERT;
    }
    if (WEGuiderTimerID)
    {
        IERmTimer(WEGuiderTimerID);
        WEGuiderTimerID = 0;
    }
    GuideStatus &= SX_CLEAR_WE;
    GuideStatus |= SX_GUIDE_WEST;
    sxSetSTAR2000(handle, GuideStatus);
    if (ms < 100)
    {
        usleep(ms * 1000);
        GuideStatus &= SX_CLEAR_WE;
        sxSetSTAR2000(handle, GuideStatus);
    }
    else
        WEGuiderTimerID = IEAddTimer(ms, WEGuiderTimerCallback, this);
    return IPS_OK;
}

IPState SXCCD::GuideEast(uint32_t ms)
{
    if (!HasST4Port || ms < 1)
    {
        return IPS_ALERT;
    }
    if (WEGuiderTimerID)
    {
        IERmTimer(WEGuiderTimerID);
        WEGuiderTimerID = 0;
    }
    GuideStatus &= SX_CLEAR_WE;
    GuideStatus |= SX_GUIDE_EAST;
    sxSetSTAR2000(handle, GuideStatus);
    if (ms < 100)
    {
        usleep(ms * 1000);
        GuideStatus &= SX_CLEAR_WE;
        sxSetSTAR2000(handle, GuideStatus);
    }
    else
        WEGuiderTimerID = IEAddTimer(ms, WEGuiderTimerCallback, this);
    return IPS_OK;
}

void SXCCD::WEGuiderTimerHit()
{
    GuideStatus &= SX_CLEAR_WE;
    sxSetSTAR2000(handle, GuideStatus);
    WEGuiderTimerID = 0;
    GuideComplete(AXIS_RA);
}

IPState SXCCD::GuideNorth(uint32_t ms)
{
    if (!HasST4Port || ms < 1)
    {
        return IPS_ALERT;
    }
    if (NSGuiderTimerID)
    {
        IERmTimer(NSGuiderTimerID);
        NSGuiderTimerID = 0;
    }
    GuideStatus &= SX_CLEAR_NS;
    GuideStatus |= SX_GUIDE_NORTH;
    sxSetSTAR2000(handle, GuideStatus);
    if (ms < 100)
    {
        usleep(ms * 1000);
        GuideStatus &= SX_CLEAR_NS;
        sxSetSTAR2000(handle, GuideStatus);
    }
    else
        NSGuiderTimerID = IEAddTimer(ms, NSGuiderTimerCallback, this);
    return IPS_OK;
}

IPState SXCCD::GuideSouth(uint32_t ms)
{
    if (!HasST4Port || ms < 1)
    {
        return IPS_ALERT;
    }
    if (NSGuiderTimerID)
    {
        IERmTimer(NSGuiderTimerID);
        NSGuiderTimerID = 0;
    }
    GuideStatus &= SX_CLEAR_NS;
    GuideStatus |= SX_GUIDE_SOUTH;
    sxSetSTAR2000(handle, GuideStatus);
    if (ms < 100)
    {
        usleep(ms * 1000);
        GuideStatus &= SX_CLEAR_NS;
        sxSetSTAR2000(handle, GuideStatus);
    }
    else
        NSGuiderTimerID = IEAddTimer(ms, NSGuiderTimerCallback, this);
    return IPS_OK;
}

void SXCCD::NSGuiderTimerHit()
{
    GuideStatus &= SX_CLEAR_NS;
    sxSetSTAR2000(handle, GuideStatus);
    NSGuiderTimerID = 0;
    GuideComplete(AXIS_DE);
}

void SXCCD::ISGetProperties(const char *dev)
{
    INDI_UNUSED(dev);
    INDI::CCD::ISGetProperties(name);
    addDebugControl();
}

bool SXCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    bool result = false;
    if (strcmp(name, ShutterSP.name) == 0)
    {
        IUUpdateSwitch(&ShutterSP, states, names, n);
        ShutterSP.s = IPS_OK;
        IDSetSwitch(&ShutterSP, nullptr);
        sxSetShutter(handle, ShutterS[0].s != ISS_ON);
        result = true;
    }
    else if (strcmp(name, CoolerSP.name) == 0)
    {
        IUUpdateSwitch(&CoolerSP, states, names, n);
        CoolerSP.s = IPS_OK;
        IDSetSwitch(&CoolerSP, nullptr);
        unsigned char status;
        unsigned short temperature;
        sxSetCooler(handle, (unsigned char)(CoolerS[0].s == ISS_ON), (unsigned short)(TemperatureRequest * 10 + 2730),
                    &status, &temperature);
        TemperatureReported = TemperatureN[0].value = (temperature - 2730) / 10.0;
        TemperatureNP.s                             = IPS_OK;
        IDSetNumber(&TemperatureNP, nullptr);
        result = true;
    }
    //    else if (strcmp(name, BayerSP.name) == 0)
    //    {
    //        IUUpdateSwitch(&BayerSP, states, names, n);
    //        BayerSP.s = IPS_OK;
    //        IDSetSwitch(&BayerSP, nullptr);
    //        if (BayerS[0].s == ISS_ON) {
    //            SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
    //            defineText(&BayerTP);
    //        }
    //        else {
    //            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
    //            deleteProperty(BayerTP.name);
    //        }
    //        result = true;
    //    }
    else
        result = INDI::CCD::ISNewSwitch(dev, name, states, names, n);
    return result;
}

//bool SXCCD::saveConfigItems(FILE *fp)
//{
//    INDI::CCD::saveConfigItems(fp);
//    IUSaveConfigSwitch(fp, &BayerSP);
//    return true;
//}
