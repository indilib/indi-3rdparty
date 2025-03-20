/*
    ZWO CAA Rotator
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "asi_rotator.h"
#include "config.h"

#include <CAA_API.h>
#include <inditimer.h>
#include <map>

static class Loader
{
        INDI::Timer hotPlugTimer;
        std::map<int, std::shared_ptr<ASICAA>> rotators;
    public:
        Loader()
        {
            load(false);
        }

    public:
        static size_t getCountOfConnectedRotators()
        {
            return size_t(std::max(CAAGetNum(), 0));
        }

        static std::vector<CAA_INFO> getConnectedRotators()
        {
            std::vector<CAA_INFO> result;
            int count = getCountOfConnectedRotators();
            for(int i = 0; i < count; i++)
            {
                int id = -1;
                if (CAAGetID(i, &id) == CAA_SUCCESS)
                {
                    CAA_INFO info;
                    if (CAAGetProperty(id, &info) == CAA_SUCCESS)
                        result.push_back(info);
                }
            }
            return result;
        }

    public:
        void load(bool isHotPlug)
        {
            auto usedRotators = std::move(rotators);
            UniqueName uniqueName(usedRotators);

            for(const auto &rotatorInfo : getConnectedRotators())
            {
                int id = rotatorInfo.ID;

                // rotator already created
                if (usedRotators.find(id) != usedRotators.end())
                {
                    std::swap(rotators[id], usedRotators[id]);
                    continue;
                }

                CAA_SN serialNumber;
                std::string serialNumberStr = "";
                if(CAAGetSerialNumber(id, &serialNumber) == CAA_SUCCESS)
                {
                    char snChars[100];
                    auto &sn = serialNumber;
                    sprintf(snChars, "%02x%02x%02x%02x%02x%02x%02x%02x", sn.id[0], sn.id[1],
                            sn.id[2], sn.id[3], sn.id[4], sn.id[5], sn.id[6], sn.id[7]);
                    snChars[16] = 0;
                    serialNumberStr = std::string(snChars);
                }

                ASICAA *asiRotator = new ASICAA(rotatorInfo.ID, uniqueName.make(rotatorInfo));
                rotators[id] = std::shared_ptr<ASICAA>(asiRotator);
                if (isHotPlug)
                    asiRotator->ISGetProperties(nullptr);
            }
        }

    public:
        class UniqueName
        {
                std::map<std::string, bool> used;
            public:
                UniqueName() = default;
                UniqueName(const std::map<int, std::shared_ptr<ASICAA>> &usedRotators)
                {
                    for (const auto &rotator : usedRotators)
                        used[rotator.second->getDeviceName()] = true;
                }

                std::string make(const CAA_INFO &rotatorInfo)
                {
                    std::string rotatorName = "ZWO CAA " + std::string(rotatorInfo.Name);
                    std::string uniqueName = rotatorName;

                    for (int index = 0; used[uniqueName] == true; )
                        uniqueName = rotatorName + " " + std::to_string(++index);

                    used[uniqueName] = true;
                    return uniqueName;
                }
        };
} loader;

ASICAA::ASICAA(int ID, const std::string &rotatorName) : m_ID(ID)
{
    setDeviceName(rotatorName.c_str());

    setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);

    setRotatorConnection(CONNECTION_NONE);

    // Can move in absolute position
    RI::SetCapability(ROTATOR_CAN_ABORT |
                      ROTATOR_CAN_REVERSE |
                      ROTATOR_CAN_SYNC);
}

bool ASICAA::initProperties()
{
    INDI::Rotator::initProperties();

    // Temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%.2f", -50, 70, 0., 0.);
    TemperatureNP.fill(getDeviceName(), "ROTATOR_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Beep
    BeepSP[INDI_ENABLED].fill("INDI_ENABLED", "Enable", ISS_OFF);
    BeepSP[INDI_DISABLED].fill("INDI_DISABLED", "Disable", ISS_ON);
    BeepSP.fill(getDeviceName(), "ROTATOR_BEEP", "Beep",
                MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Version Info
    VersionInfoTP[VERSION_FIRMWARE].fill("FIRMWARE_VERSION", "Firmware", "Unknown");
    VersionInfoTP[VERSION_SDK].fill("SDK_VERSION", "SDK", "Unknown");
    VersionInfoTP[VERSION_SERIAL].fill("SERIAL_NUMBER", "Serial Number", "Unknown");
    VersionInfoTP.fill(getDeviceName(), "VERSION_INFO", "Version", INFO_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();
    addSimulationControl();

    setDefaultPollingPeriod(500);

    return true;
}

bool ASICAA::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(BeepSP);

        // Get firmware version
        unsigned char major, minor, build;
        char versionStr[32];
        if (CAAGetFirmwareVersion(m_ID, &major, &minor, &build) == CAA_SUCCESS)
        {
            snprintf(versionStr, sizeof(versionStr), "%d.%d.%d", major, minor, build);
            VersionInfoTP[0].setText(versionStr);
        }

        // Get SDK version
        VersionInfoTP[1].setText(CAAGetSDKVersion());

        // Get serial number
        CAA_SN serialNumber;
        if (CAAGetSerialNumber(m_ID, &serialNumber) == CAA_SUCCESS)
        {
            char snStr[32];
            snprintf(snStr, sizeof(snStr), "%02x%02x%02x%02x%02x%02x%02x%02x",
                     serialNumber.id[0], serialNumber.id[1], serialNumber.id[2], serialNumber.id[3],
                     serialNumber.id[4], serialNumber.id[5], serialNumber.id[6], serialNumber.id[7]);
            VersionInfoTP[2].setText(snStr);
        }

        defineProperty(VersionInfoTP);
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(BeepSP);
        deleteProperty(VersionInfoTP);
    }

    return true;
}

const char * ASICAA::getDefaultName()
{
    return "ZWO CAA";
}

bool ASICAA::Connect()
{
    if (isSimulation())
    {
        LOG_INFO("Simulation connected.");
        return true;
    }

    CAA_ERROR_CODE code = CAAOpen(m_ID);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to open rotator: %d", code);
        return false;
    }

    // Get current position
    float angle = 0;
    code = CAAGetDegree(m_ID, &angle);
    if (code == CAA_SUCCESS)
        GotoRotatorNP[0].setValue(angle);

    // Get current beep state
    bool beepEnabled = false;
    code = CAAGetBeep(m_ID, &beepEnabled);
    if (code == CAA_SUCCESS)
    {
        BeepSP[INDI_ENABLED].setState(beepEnabled ? ISS_ON : ISS_OFF);
        BeepSP[INDI_DISABLED].setState(beepEnabled ? ISS_OFF : ISS_ON);
        BeepSP.setState(IPS_OK);
    }
    else
        BeepSP.setState(IPS_ALERT);


    // Get current reverse state
    bool reverseEnabled = false;
    code = CAAGetReverse(m_ID, &reverseEnabled);
    if (code == CAA_SUCCESS)
    {
        ReverseRotatorSP[INDI_ENABLED].setState(reverseEnabled ? ISS_ON : ISS_OFF);
        ReverseRotatorSP[INDI_DISABLED].setState(reverseEnabled ? ISS_OFF : ISS_ON);
        ReverseRotatorSP.setState(IPS_OK);
    }
    else
        ReverseRotatorSP.setState(IPS_ALERT);

    // Get max degree limit
    float maxDegree = 0;
    code = CAAGetMaxDegree(m_ID, &maxDegree);
    if (code == CAA_SUCCESS)
    {
        RotatorLimitsNP[0].setValue(maxDegree);
        RotatorLimitsNP.setState(IPS_OK);
    }
    else
        RotatorLimitsNP.setState(IPS_ALERT);

    SetTimer(getCurrentPollingPeriod());

    return true;
}

bool ASICAA::Disconnect()
{
    if (!isSimulation())
        CAAClose(m_ID);
    return true;
}

bool ASICAA::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RotatorLimitsNP.isNameMatch(name))
        {
            if (!isSimulation())
            {
                CAA_ERROR_CODE code = CAASetMaxDegree(m_ID, values[0]);
                if (code != CAA_SUCCESS)
                {
                    LOGF_ERROR("Failed to set max degree limit: %d", code);
                    RotatorLimitsNP.setState(IPS_ALERT);
                    RotatorLimitsNP.apply();
                    return true;
                }
            }

            RotatorLimitsNP.update(values, names, n);
            RotatorLimitsNP.setState(IPS_OK);
            RotatorLimitsNP.apply();
            return true;
        }
    }

    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

bool ASICAA::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (BeepSP.isNameMatch(name))
        {
            if (!isSimulation())
            {
                CAA_ERROR_CODE code = CAASetBeep(m_ID, strcmp(BeepSP[0].getName(), names[0]) == 0);
                if (code != CAA_SUCCESS)
                {
                    LOGF_ERROR("Failed to set beep state: %d", code);
                    BeepSP.setState(IPS_ALERT);
                    BeepSP.apply();
                    return true;
                }
            }

            BeepSP.update(states, names, n);
            BeepSP.setState(IPS_OK);
            BeepSP.apply();
            return true;
        }
    }

    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

IPState ASICAA::MoveRotator(double angle)
{
    if (isSimulation())
    {
        GotoRotatorNP[0].setValue(angle);
        return IPS_OK;
    }

    // Get current position
    float currentAngle = 0;
    CAA_ERROR_CODE code = CAAGetDegree(m_ID, &currentAngle);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to read current angle: %d", code);
        return IPS_ALERT;
    }

    // Check if target angle exceeds max limit
    if (angle > RotatorLimitsNP[0].getValue())
    {
        LOGF_ERROR("Target angle %.2f exceeds max limit %.2f", angle,
                   RotatorLimitsNP[0].getValue());
        return IPS_ALERT;
    }

    // If target is very close to current position, return immediately
    if (fabs(currentAngle - angle) <= THRESHOLD)
    {
        GotoRotatorNP[0].setValue(currentAngle);
        return IPS_OK;
    }

    code = CAAMoveTo(m_ID, angle);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to move rotator: %d", code);
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

bool ASICAA::AbortRotator()
{
    if (isSimulation())
        return true;

    CAA_ERROR_CODE code = CAAStop(m_ID);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to abort rotation: %d", code);
        return false;
    }

    return true;
}

bool ASICAA::SyncRotator(double angle)
{
    if (isSimulation())
    {
        GotoRotatorNP[0].setValue(angle);
        return true;
    }

    CAA_ERROR_CODE code = CAACurDegree(m_ID, angle);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to sync rotator: %d", code);
        return false;
    }

    return true;
}

bool ASICAA::ReverseRotator(bool enabled)
{
    if (isSimulation())
        return true;

    CAA_ERROR_CODE code = CAASetReverse(m_ID, enabled);
    if (code != CAA_SUCCESS)
    {
        LOGF_ERROR("Failed to reverse rotator: %d", code);
        return false;
    }

    return true;
}

void ASICAA::TimerHit()
{
    if (!isConnected())
        return;

    bool propertyUpdated = false;

    // Get temperature
    float temperature = 0;
    if (!isSimulation() && CAAGetTemp(m_ID, &temperature) == CAA_SUCCESS)
    {
        if (fabs(temperature - TemperatureNP[0].getValue()) > THRESHOLD)
        {
            TemperatureNP[0].setValue(temperature);
            TemperatureNP.setState(IPS_OK);
            TemperatureNP.apply();
        }
    }

    // Always get current angle
    float currentAngle = 0;
    if (!isSimulation() && CAAGetDegree(m_ID, &currentAngle) == CAA_SUCCESS)
    {
        // Update position if change is significant
        if (fabs(currentAngle - GotoRotatorNP[0].getValue()) > THRESHOLD)
        {
            GotoRotatorNP[0].setValue(currentAngle);
            propertyUpdated = true;
        }
    }

    // Check if we're moving
    bool isMoving = false;
    bool isHandControl = false;
    if (!isSimulation() && CAAIsMoving(m_ID, &isMoving, &isHandControl) == CAA_SUCCESS)
    {
        m_IsMoving = isMoving;
        m_IsHandControl = isHandControl;

        if (m_IsMoving && GotoRotatorNP.getState() != IPS_BUSY)
        {
            GotoRotatorNP.setState(IPS_BUSY);
            propertyUpdated = true;
        }
        else if (!m_IsMoving && GotoRotatorNP.getState() == IPS_BUSY)
        {
            GotoRotatorNP.setState(IPS_OK);
            propertyUpdated = true;
            LOG_INFO("Rotation complete.");
        }
    }

    if (propertyUpdated)
        GotoRotatorNP.apply();

    SetTimer(getCurrentPollingPeriod());
}
