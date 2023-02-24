/*
    PlayerOne CCD Driver (Based on ASI Single CCD Driver)

    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2018 Leonard Bottleman (leonard@whiteweasel.net)
    Copyright (C) 2021 Pawel Soja (kernel32.pl@gmail.com)
    Copyright (C) 2021 Hiroshi Saito (hiro3110g@gmail.com)

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

#include "playerone_single_ccd.h"
#include "config.h"

#include <pwd.h>
#include <unistd.h>
#include <map>

#ifdef USE_SIMULATION
static int _POAGetCameraCount()
{
    return 2;
}

static POAErrors _POAGetCameraProperties(int iCameraIndex, POACameraProperties *pPOACameraInfo)
{
    INDI_UNUSED(iCameraIndex);
    strncpy(pPOACameraInfo->cameraModelName, "    SIMULATE", sizeof(pPOACameraInfo->cameraModelName));
    return POA_OK;
}
#else
# define _POAGetCameraCount POAGetCameraCount
# define _POAGetCameraProperties POAGetCameraProperties
#endif

static std::unique_ptr<POASingleCamera> poaCamera(new POASingleCamera());

size_t POASingleCamera::getCountOfConnectedCameras()
{
    return size_t(std::max(_POAGetCameraCount(), 0));
}

std::vector<POACameraProperties> POASingleCamera::getConnectedCameras()
{
    std::vector<POACameraProperties> result(getCountOfConnectedCameras());
    int i = 0;
    for(auto &cameraInfo : result)
        _POAGetCameraProperties(i++, &cameraInfo);
    return result;
}

POASingleCamera::POASingleCamera() : POABase(), CamerasListFile(GetHomeDirectory() + "/.indi/PlayerOneCameras.xml")
{

}

std::string POASingleCamera::GetHomeDirectory() const
{
    // Check first the HOME environmental variable
    const char *HomeDir = getenv("HOME");

    // ...otherwise get the home directory of the current user.
    if (!HomeDir)
    {
        HomeDir = getpwuid(getuid())->pw_dir;
    }
    return (HomeDir ? std::string(HomeDir) : "");
}

//////////////////////////////////////////////////////////////////////////////////
/// We have three XML entries with the following labels:
/// PlayerOne Camera 1
/// PlayerOne Camera 2
/// PlayerOne Camera 3
/// When the driver is created, the above label are used to set the initial device
/// name. From this, we check the PlayerOneCameras.xml file which should have the following
/// structure.
/// <Camera id="PlayerOne Camera 1">Sedna-M</Camera>
/// <Camera id="PlayerOne Camera 2">Poseidon-C</Camera>
/// <Camera id="PlayerOne Camera 3"></Camera>
/// Therefore, we match the ID against the current device name. For example, if current
/// driver label is "PlayerOne Camera 1", when check the list of connected cameras for "Sedna-M"
/// and if found, we connect to it.
//////////////////////////////////////////////////////////////////////////////////
bool POASingleCamera::loadCamerasList()
{
    LilXML *XmlHandle = newLilXML();
    FILE *file = fopen(CamerasListFile.c_str(), "r");
    XMLEle *RootXmlNode = nullptr, *CurrentXmlNode = nullptr;
    char errorMessage[512] = {0};
    if (file)
    {
        RootXmlNode = readXMLFile(file, XmlHandle, errorMessage);
        fclose(file);
    }
    delLilXML(XmlHandle);

    // No file detected, let's create one.
    if (RootXmlNode == nullptr)
    {
        delXMLEle(RootXmlNode);
        XMLEle *oneElement = nullptr;
        file = fopen(CamerasListFile.c_str(), "w");
        if (!file)
            return false;

        RootXmlNode = addXMLEle(nullptr, "PlayerOneCameras");

        for (int i = 0; i < 3; i++)
        {
            oneElement = addXMLEle(RootXmlNode, "Camera");
            char id[MAXINDINAME] = {0};
            snprintf(id, MAXINDINAME, "PlayerOne Camera %d", i + 1);
            addXMLAtt(oneElement, "id", id);
            m_ConfigCameras[id] = "";
        }

        prXMLEle(file, RootXmlNode, 0);
        fclose(file);
        delXMLEle(RootXmlNode);
        return true;
    }

    m_ConfigCameras.clear();
    CurrentXmlNode = nextXMLEle(RootXmlNode, 1);
    // Find the current telescope in the config file
    while (CurrentXmlNode)
    {
        const char *id = findXMLAttValu(CurrentXmlNode, "id");
        m_ConfigCameras[id] = pcdataXMLEle(CurrentXmlNode);
        CurrentXmlNode = nextXMLEle(RootXmlNode, 0);
    }

    delXMLEle(RootXmlNode);
    return !m_ConfigCameras.empty();
}

//////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////
bool POASingleCamera::saveCamerasList()
{
    XMLEle *RootXmlNode = nullptr;
    XMLEle *oneElement = nullptr;

    FILE *file = fopen(CamerasListFile.c_str(), "w");

    RootXmlNode = addXMLEle(nullptr, "PlayerOneCameras");

    for (const auto &kv : m_ConfigCameras)
    {
        oneElement = addXMLEle(RootXmlNode, "Camera");
        addXMLAtt(oneElement, "id", kv.first.c_str());
        editXMLEle(oneElement, kv.second.c_str());
    }

    prXMLEle(file, RootXmlNode, 0);
    fclose(file);
    delXMLEle(RootXmlNode);
    return true;
}

bool POASingleCamera::initCameraFromConfig()
{
    size_t detectedCameras = getCountOfConnectedCameras();
    if (detectedCameras == 0)
        return false;

    // Now get a list of all connected cameras.
    std::vector<POACameraProperties> connectedCameras = getConnectedCameras();
    CamerasSP.reserve(connectedCameras.size());
    for (size_t i = 0; i < connectedCameras.size(); i++)
    {
        INDI::WidgetSwitch node;
        node.fill(connectedCameras[i].cameraModelName, connectedCameras[i].cameraModelName, ISS_OFF);
        CamerasSP.push(std::move(node));
    }

    if (loadCamerasList())
    {
        // If INDIDEV was not set and we just have a generic camera "PlayerOne Camera"
        // then we use the first camera detected if available.
        if (!strcmp(getDeviceName(), "PlayerOne Camera") && connectedCameras.size() > 0)
        {
            mCameraInfo = connectedCameras[0];
            mCameraName = PLAYERONE_PREFIX + std::string(mCameraInfo.cameraModelName);

            CamerasSP.fill(mCameraName.c_str(), "CAMERAS_LIST", "Cameras", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
            CamerasSP[0].setState(ISS_ON);
            return true;
        }

        // Otherwise, check the config file (~/.indi/PlayerOneCameras.xml) entries
        // For each camera (PlayerOne Camera 1, PlayerOne Camera 2, PlayerOne Camera 3)
        // we can have a specific camera assigned to them. If a specific camera is set, let's try
        // to connect to this particular camera. Otherwise, take them in order as detected by connectedCameras
        for (const auto &kv : m_ConfigCameras)
        {
            // Match ID (PlayerOne Camera 1, PlayerOne Camera 2..etc)
            if (kv.first == getDeviceName())
            {
                mCameraID = kv.first;
                // Get index. i.e. PlayerOne Camera 1 --> index = 0
                const uint32_t index = static_cast<uint32_t>(mCameraID.back()) - 0x31;
                // Now find out which camera we're supposed to connect to?
                std::string configCamera = kv.second;
                // Do we have a specific camera selected?
                if (!configCamera.empty())
                {
                    for (size_t i = 0; i < connectedCameras.size(); i++)
                    {
                        if (configCamera == connectedCameras[i].cameraModelName)
                        {
                            mCameraInfo = connectedCameras[i];
                            mCameraName = PLAYERONE_PREFIX + configCamera;

                            CamerasSP.fill(configCamera.c_str(), "CAMERAS_LIST", "Cameras", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
                            CamerasSP[i].setState(ISS_ON);
                            return true;
                        }
                    }
                }
                // If this is the first time and no camera was assigned, pick up the first camera corresponding
                // to this index.
                else if (index < connectedCameras.size())
                {
                    mCameraInfo = connectedCameras[index];
                    mCameraName = PLAYERONE_PREFIX + std::string(mCameraInfo.cameraModelName);

                    CamerasSP.fill(mCameraName.c_str(), "CAMERAS_LIST", "Cameras", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);
                    CamerasSP[index].setState(ISS_ON);
                    return true;
                }
            }
        }
    }

    CamerasSP.fill(getDeviceName(), "CAMERAS_LIST", "Cameras", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    return false;
}

const char *POASingleCamera::getDefaultName()
{
    return "PlayerOne Camera";
}

void POASingleCamera::ISGetProperties(const char *dev)
{
    POABase::ISGetProperties(dev);
    CamerasSP.define();
}

bool POASingleCamera::initProperties()
{
    if (initCameraFromConfig())
    {
        m_ConfigCameraFound = true;

        setDeviceName(mCameraName.c_str());
    }

    return POABase::initProperties();
}

bool POASingleCamera::Connect()
{
    if (!m_ConfigCameraFound)
    {
        LOG_WARN("Failed to find camera. Please check USB and power connections.");
        return false;
    }

    return POABase::Connect();

}

bool POASingleCamera::ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (CamerasSP.isNameMatch(name))
        {
            CamerasSP.update(states, names, n);
            const char *targetCamera = CamerasSP.findOnSwitch()->getLabel();
            m_ConfigCameras[mCameraID] = targetCamera;
            CamerasSP.setState(IPS_OK);
            saveCamerasList();
            LOG_INFO("The driver must now be restarted for this change to take effect.");
            CamerasSP.apply();
            return true;
        }
    }

    return POABase::ISNewSwitch(dev, name, states, names, n);
}
