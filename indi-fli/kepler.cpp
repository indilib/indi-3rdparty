/*
    INDI Driver for Kepler sCMOS camera.
    Copyright (C) 2022 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "config.h"
#include "kepler.h"

#include <memory>

static std::unique_ptr<Kepler> kepler(new Kepler());

Kepler::Kepler()
{
    setVersion(FLI_CCD_VERSION_MAJOR, FLI_CCD_VERSION_MINOR);
}

Kepler::~Kepler()
{

}

const char *Kepler::getDefaultName()
{
    return "FLI Kepler";
}

bool Kepler::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    CaptureFormat mono = {"INDI_MONO", "Mono", 16, true};
    addCaptureFormat(mono);

    SetCCDCapability(CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER);

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.04, 3600, 1, false);

    addAuxControls();

    return true;
}

void Kepler::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
    defineProperty(&CommunicationMethod);
}

bool Kepler::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {

    }
    else
    {

    }

    return true;
}

bool Kepler::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool Kepler::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {

    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool Kepler::Connect()
{
    return true;
}

bool Kepler::Disconnect()
{
    return true;
}

bool Kepler::establishConnection()
{
    return true;
}

bool Kepler::setup()
{
    //    SetCCDParams(FLICam.Visible_Area[2] - FLICam.Visible_Area[0], FLICam.Visible_Area[3] - FLICam.Visible_Area[1], 16,
    //                 FLICam.x_pixel_size, FLICam.y_pixel_size);

    //    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    //    PrimaryCCD.setFrameBufferSize(nbuf);
    return true;
}

int Kepler::SetTemperature(double temperature)
{
    return 0;
}

bool Kepler::StartExposure(float duration)
{
    PrimaryCCD.setExposureDuration(duration);
    InExposure = true;
    return true;
}

bool Kepler::AbortExposure()
{
    InExposure = false;
    return true;
}

bool Kepler::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{
    return true;
}

bool Kepler::UpdateCCDFrame(int x, int y, int w, int h)
{

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    int nbuf = (w / PrimaryCCD.getBinX()) * (h / PrimaryCCD.getBinY()) * (PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool Kepler::UpdateCCDBin(int binx, int biny)
{
    PrimaryCCD.setBin(binx, biny);
    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

bool Kepler::download()
{
    std::unique_lock<std::mutex> guard(ccdBufferLock);

    guard.unlock();

    ExposureComplete(&PrimaryCCD);

    return true;
}

void Kepler::TimerHit()
{
    if (isConnected() == false)
        return;

    if (InExposure)
    {

    }

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            break;

        case IPS_BUSY:
            break;

        case IPS_ALERT:
            break;
    }
}

bool Kepler::saveConfigItems(FILE *fp)
{
    // Save CCD Config
    INDI::CCD::saveConfigItems(fp);


    return true;
}

void Kepler::debugTriggered(bool enable)
{

}
