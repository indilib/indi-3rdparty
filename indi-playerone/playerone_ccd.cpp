/*
    PlayerOne CCD Driver (Based on ASI CCD Driver)

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

#include "playerone_ccd.h"
#include <unistd.h>
#include <pwd.h>

#include <map>
//#define USE_SIMULATION

#ifdef USE_SIMULATION
static int _POAGetCameraCount()
{
    return 2;
}

static POAErrors _POAGetCameraProperties(POACameraProperties *pPOACameraInfo, int iCameraIndex)
{
    INDI_UNUSED(iCameraIndex);
    strncpy(pPOACameraInfo->cameraModelName, "    SIMULATE", sizeof(pPOACameraInfo->cameraModelName));
    return POA_OK;
}
#else
# define _POAGetCameraCount POAGetCameraCount
# define _POAGetCameraProperties POAGetCameraProperties
#endif

static class Loader
{
        INDI::Timer hotPlugTimer;
        std::map<int, std::shared_ptr<POACCD>> cameras;
    public:
        Loader()
        {
            load(false);

            // JM 2021-04-03: Some users reported camera dropping out since hotplug was introduced.
            // Disabling it for now until more investigation is conduced.
            //            hotPlugTimer.start(1000);
            //            hotPlugTimer.callOnTimeout([&]
            //            {
            //                if (getCountOfConnectedCameras() != cameras.size())
            //                {
            //                    load(true);
            //                }
            //            });
        }

    public:
        static size_t getCountOfConnectedCameras()
        {
            return size_t(std::max(_POAGetCameraCount(), 0));
        }

        static std::vector<POACameraProperties> getConnectedCameras()
        {
            std::vector<POACameraProperties> result(getCountOfConnectedCameras());
            int i = 0;
            for(auto &cameraInfo : result)
                _POAGetCameraProperties(i++, &cameraInfo);
            return result;
        }

    public:
        void load(bool isHotPlug)
        {
            auto usedCameras = std::move(cameras);

            UniqueName uniqueName(usedCameras);

            for(const auto &cameraInfo : getConnectedCameras())
            {
                int id = cameraInfo.cameraID;

                // camera already created
                if (usedCameras.find(id) != usedCameras.end())
                {
                    std::swap(cameras[id], usedCameras[id]);
                    continue;
                }

                std::string serialNumberStr = "";
                if(POAOpenCamera(id) == POA_OK)
                {
                    POACameraProperties props;
                    if (POAGetCameraPropertiesByID(id, &props) == POA_OK)
                    {
                        POACloseCamera(id);
                        serialNumberStr = std::string(props.SN);
                    }
                }

                POACCD *poaCcd = new POACCD(cameraInfo, uniqueName.make(cameraInfo), serialNumberStr);
                cameras[id] = std::shared_ptr<POACCD>(poaCcd);
                if (isHotPlug)
                    poaCcd->ISGetProperties(nullptr);
            }
        }

    public:
        class UniqueName
        {
                std::map<std::string, bool> used;
            public:
                UniqueName() = default;
                UniqueName(const std::map<int, std::shared_ptr<POACCD>> &usedCameras)
                {
                    for (const auto &camera : usedCameras)
                        used[camera.second->getDeviceName()] = true;
                }

                std::string make(const POACameraProperties &cameraInfo)
                {
                    std::string cameraName = PLAYERONE_CCD_PREFIX + std::string(cameraInfo.cameraModelName);
                    std::string uniqueName = cameraName;

                    for (int index = 0; used[uniqueName] == true; )
                        uniqueName = cameraName + " " + std::to_string(++index);

                    used[uniqueName] = true;
                    return uniqueName;
                }
        };
} loader;

namespace
{

// trim from start (in place)
static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

std::string GetHomeDirectory()
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

}  // namespace

// Nicknames are stored in an xml-format NICKNAME_FILE in a format like the below.
// Nicknames are assoicated with the serial number of the camera, and are entered/changed
// with NicknameTP. Since the device-name can't be changed once the driver is running,
// changes to nicknames can only take effect at the next INDI startup.
// <Nicknames>
//  <Nickname id="serialNumber1">nickname1</Nickname>
//  <Nickname id="serialNumber2">nickname2</Nickname>
//  <Nickname id="serialNumber3">nickname3</Nickname>
// </Nicknames>

#define ROOTNODE "Nicknames"
#define ENTRYNODE "Nickname"
#define ATTRIBUTE "SerialNumber"

void POACCD::loadNicknames()
{
    const std::string filename = GetHomeDirectory() + NICKNAME_FILE;
    mNicknames.clear();

    LilXML *xmlHandle = newLilXML();
    XMLEle *rootXmlNode = nullptr;
    char errorMessage[512] = {0};
    FILE *file = fopen(filename.c_str(), "r");
    if (file)
    {
        rootXmlNode = readXMLFile(file, xmlHandle, errorMessage);
        fclose(file);
    }
    delLilXML(xmlHandle);

    if (rootXmlNode == nullptr)
        return;

    XMLEle *currentXmlNode = nextXMLEle(rootXmlNode, 1);
    while (currentXmlNode)
    {
        const char *id = findXMLAttValu(currentXmlNode, ATTRIBUTE);
        if (id != nullptr)
        {
            std::string name = pcdataXMLEle(currentXmlNode);
            if (!name.empty())
                trim(name);
            if (!name.empty())
                mNicknames[id] = name;
        }
        currentXmlNode = nextXMLEle(rootXmlNode, 0);
    }

    delXMLEle(rootXmlNode);
}

void POACCD::saveNicknames()
{
    const std::string filename = GetHomeDirectory() + NICKNAME_FILE;
    XMLEle *rootXmlNode = nullptr;
    XMLEle *oneElement = nullptr;

    FILE *file = fopen(filename.c_str(), "w");

    rootXmlNode = addXMLEle(nullptr, ROOTNODE);

    for (const auto &kv : mNicknames)
    {
        oneElement = addXMLEle(rootXmlNode, ENTRYNODE);
        addXMLAtt(oneElement, ATTRIBUTE, kv.first.c_str());
        editXMLEle(oneElement, kv.second.c_str());
    }

    prXMLEle(file, rootXmlNode, 0);
    fclose(file);
    delXMLEle(rootXmlNode);
}


bool POACCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (NicknameTP.isNameMatch(name))
        {
            NicknameTP.update(texts, names, n);
            NicknameTP.setState(IPS_OK);
            NicknameTP.apply();
            if (!mSerialNumber.empty())
            {
                loadNicknames();  // another camera may have updated its nickname.
                std::string newNickname = texts[0];
                trim(newNickname);
                if (newNickname.empty())
                {
                    mNicknames.erase(mSerialNumber);
                    LOGF_INFO("Nickname for %s removed.", mSerialNumber.c_str());
                }
                else
                {
                    mNicknames[mSerialNumber] = newNickname;
                    LOGF_INFO("Nickname for %s changed to %s.", mSerialNumber.c_str(), newNickname.c_str());
                }
                saveNicknames();
                LOG_INFO("The driver must now be restarted for this change to take effect.");
            }
            else
            {
                LOG_INFO("Can't apply nickname change--serial number not known.");
            }
            NicknameTP.apply();
            return true;
        }
    }
    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

///////////////////////////////////////////////////////////////////////
/// Constructor for multi-camera driver.
///////////////////////////////////////////////////////////////////////
POACCD::POACCD(const POACameraProperties &camInfo, const std::string &cameraName,
               const std::string &serialNumber) : POABase()
{
    mCameraInfo = camInfo;
    mSerialNumber = serialNumber;

    loadNicknames();
    if (!serialNumber.empty())
    {
        auto nickname = mNicknames[mSerialNumber];
        if (!nickname.empty())
        {
            // check prefix
            std::string prefix = PLAYERONE_CCD_PREFIX;
            if (nickname.compare(0, prefix.size(), prefix) != 0)
            {
                nickname = prefix + nickname;
            }
            setDeviceName(nickname.c_str());
            mCameraName = nickname;
            mNickname = nickname;
            LOGF_INFO("Using nickname %s for serial number %s.", nickname.c_str(), mSerialNumber.c_str());
            return;
        }
    }

    setDeviceName(cameraName.c_str());
    mCameraName = cameraName;
}

