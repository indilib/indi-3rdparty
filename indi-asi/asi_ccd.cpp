/*
    ASI CCD Driver

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)

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

#include "asi_ccd.h"
#include <unistd.h>
#include <pwd.h>

#include <hotplugmanager.h>
#include "asi_ccd_hotplug_handler.h"
//#define USE_SIMULATION

#ifdef USE_SIMULATION
static int _ASIGetNumOfConnectedCameras()
{
    return 2;
}

static ASI_ERROR_CODE _ASIGetCameraProperty(ASI_CAMERA_INFO *pASICameraInfo, int iCameraIndex)
{
    INDI_UNUSED(iCameraIndex);
    strncpy(pASICameraInfo->Name, "    SIMULATE", sizeof(pASICameraInfo->Name));
    return ASI_SUCCESS;
}
#else
# define _ASIGetNumOfConnectedCameras ASIGetNumOfConnectedCameras
# define _ASIGetCameraProperty ASIGetCameraProperty
#endif

static class Loader
{
        std::shared_ptr<INDI::ASICCDHotPlugHandler> hotPlugHandler;
    public:
        Loader()
        {
            hotPlugHandler = std::make_shared<INDI::ASICCDHotPlugHandler>();
            INDI::HotPlugManager::getInstance().registerHandler(hotPlugHandler);
            INDI::HotPlugManager::getInstance().start(1000); // Start hot-plug checks every 1 second
        }
} loader;

bool ASICCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

///////////////////////////////////////////////////////////////////////
/// Constructor for multi-camera driver.
///////////////////////////////////////////////////////////////////////
ASICCD::ASICCD(const ASI_CAMERA_INFO &camInfo, const std::string &cameraName,
               const std::string &serialNumber)
    : ASIBase(camInfo, serialNumber)
{
    setDeviceName(cameraName.c_str());

    if (!mSerialNumber.empty())
        setDeviceNicknameFromId(mSerialNumber.c_str());

    mCameraName = getDeviceName();
}
