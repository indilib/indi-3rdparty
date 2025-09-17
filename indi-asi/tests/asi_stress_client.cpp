#include "baseclient.h"
#include "basedevice.h"
#include "indiccd.h"
#include "indiproperty.h"
#include "indilogger.h"

#include <iostream>
#include <string>
#include <unistd.h> // For usleep

class ASIStressClient : public INDI::BaseClient
{
    public:
        ASIStressClient();
        ~ASIStressClient() = default;

        void startClient();

    protected:
        void newDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void updateProperty(INDI::Property property) override;
        void newMessage(INDI::BaseDevice baseDevice, int messageID) override;
        void serverDisconnected(int exitCode) override;

    private:
        INDI::BaseDevice mASICCD;
        INDI::Property m_ExposureProperty;
        bool ccdConnected = false;
        bool firstExposureTriggered = false;
        int m_exposureCount; // Counter for successful exposures

        void triggerExposure();
};

ASIStressClient::ASIStressClient() : m_exposureCount(0)
{
    setServer("localhost", 7624);
}

void ASIStressClient::startClient()
{
    if (!connectServer())
    {
        std::cerr << "Failed to connect to INDI server." << std::endl;
        return;
    }

    std::cout << "Connected to INDI server. Waiting for ASI CCD device..." << std::endl;

    // Keep the client alive by waiting for user input
    std::cout << "Press Enter to terminate client..." << std::endl;
    std::cin.get();
}

void ASIStressClient::triggerExposure()
{
    std::cout << "Triggering exposure....." << std::endl;
    m_ExposureProperty.getNumber()->at(0)->setValue(0.2);
    sendNewNumber(m_ExposureProperty);
}

void ASIStressClient::newDevice(INDI::BaseDevice dp)
{
    std::string deviceName = dp.getDeviceName();
    if (deviceName.rfind("ZWO CCD", 0) == 0) // Check if device name starts with "ZWO CCD"
    {
        mASICCD = dp;
        DEBUGFDEVICE("ASIStressClient", INDI::Logger::DBG_DEBUG, "Found ZWO CCD device: %s", deviceName.c_str());

        // Check if the device is connected, if not, connect it
    }
}

void ASIStressClient::newProperty(INDI::Property property)
{
    DEBUGFDEVICE("ASI", INDI::Logger::DBG_SESSION, "Received new property %s", property.getName());

    if (property.isNameMatch("CONNECTION"))
    {
        auto svp = INDI::PropertySwitch(property);
        if (svp[0].isNameMatch("CONNECT") && svp[0].getState() == ISS_OFF)
        {
            svp[0].setState(ISS_ON);
            sendNewSwitch(svp);
            DEBUGDEVICE("ASIStressClient", INDI::Logger::DBG_DEBUG, "Connecting to ZWO CCD device.");
        }
    }
    else if (property.isNameMatch("CCD_EXPOSURE"))
    {
        m_ExposureProperty = property;
        triggerExposure();
    }
    else if (property.isNameMatch("SCOPE_INFO"))
    {
        INDI::PropertyNumber nvp(property);
        // Set focal length and aperture
        nvp[0].setValue(900);
        nvp[1].setValue(120);
        sendNewNumber(nvp);
    }
}

void ASIStressClient::updateProperty(INDI::Property property)
{
    if (property.isNameMatch("CCD_EXPOSURE"))
    {
        auto exposureNVP = property.getNumber();
        DEBUGFDEVICE("ASIStressClient", INDI::Logger::DBG_SESSION, "Exposure %.2f state %s", exposureNVP->at(0)->getValue(), pstateStr(exposureNVP->getState()));
        if (exposureNVP && exposureNVP->getState() == IPS_OK)
        {
            m_exposureCount++;
            DEBUGFDEVICE("ASIStressClient", INDI::Logger::DBG_DEBUG, "Exposure completed. Total exposures: %d. Triggering next exposure.", m_exposureCount);
            triggerExposure();
        }
    }
}

void ASIStressClient::serverDisconnected(int exitCode)
{
    INDI_UNUSED(exitCode);
    ccdConnected = false;
    firstExposureTriggered = false;
    DEBUGFDEVICE("ASIStressClient", INDI::Logger::DBG_DEBUG, "Server disconnected with exit code %d", exitCode);
}

void ASIStressClient::newMessage(INDI::BaseDevice dp, int messageID)
{
    DEBUGDEVICE("ASIStressClient", INDI::Logger::DBG_SESSION, dp.messageQueue(messageID).c_str());
}

int main(int argc, char *argv[])
{
    ASIStressClient client;
    client.startClient();
    return 0;
}
