/*
    DragonFly Dome

    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include <memory>
#include <chrono>
#include <indidome.h>
#include <indioutputinterface.h>

using DD = INDI::DefaultDevice;


class DragonFlyDome : public INDI::Dome, public INDI::OutputInterface
{
    public:
        DragonFlyDome();
        virtual ~DragonFlyDome() override = default;

        const char *getDefaultName() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[],
                               int n) override; // Add ISNewText override

    protected:
        // Declare implementations for OutputInterface pure virtual methods
        virtual bool UpdateDigitalOutputs() override;
        virtual bool CommandOutput(uint32_t index, INDI::OutputInterface::OutputState command) override;

        bool Handshake() override;
        void TimerHit() override;

        // Motion
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;

        // Backlash
        virtual bool SetBacklash(int32_t steps) override;
        virtual bool SetBacklashEnabled(bool enabled) override;

        // Abort
        virtual bool Abort() override;

        // Config
        virtual bool saveConfigItems(FILE * fp) override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Set & Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool echo();

        ///////////////////////////////////////////////////////////////////////////////
        /// Relays
        ///////////////////////////////////////////////////////////////////////////////
        // bool setRelayEnabled(uint8_t id, bool enabled); // To be removed
        bool setRoofOpen(bool enabled);
        bool setRoofClose(bool enabled);
        // bool updateRelays(); // To be removed

        ///////////////////////////////////////////////////////////////////////////////
        /// Roof state supervision (called on every fresh sensor read)
        ///////////////////////////////////////////////////////////////////////////////
        void checkRoofState();
        void handleCloseFailure(const char *reason);
        void retryClose();
        bool travelTimedOut(std::chrono::steady_clock::time_point now) const;

        ///////////////////////////////////////////////////////////////////////////////
        /// Sensors
        ///////////////////////////////////////////////////////////////////////////////
        bool isSensorOn(uint8_t id);
        bool updateSensors();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, int32_t &res);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        // Peripheral Ports
        INDI::PropertySwitch PerPortSP {3};
        enum { PORT_MAIN, PORT_EXP, PORT_THIRD };

        // Roof Control Relays
        INDI::PropertyNumber DomeControlRelayNP {2};
        enum
        {
            RELAY_OPEN,
            RELAY_CLOSE,
        };

        // Roof Control Sensors
        INDI::PropertyNumber DomeControlSensorNP {2};
        enum
        {
            SENSOR_UNPARKED,
            SENSOR_PARKED,
        };

        // All Sensors
        INumberVectorProperty SensorNP;
        INumber SensorN[8];

        // Firmware Version
        INDI::PropertyText FirmwareVersionTP {1};

        // Automatic close retries after the roof reopened on its own
        INDI::PropertyNumber CloseRetryNP {1};

        // Maximum roof travel time before it is considered stopped mid-way
        INDI::PropertyNumber TravelTimeoutNP {1};

        ///////////////////////////////////////////////////////////////////////
        /// Private Variables
        ///////////////////////////////////////////////////////////////////////
        uint32_t m_UpdateRelayCounter {0};
        uint32_t m_UpdateSensorCounter {0};

        // Consecutive reads with the parked sensor ON while closing
        uint8_t m_ParkedConfirmCount {0};
        // Consecutive reads with the parked sensor OFF while parked
        uint8_t m_ParkLostCount {0};
        // Consecutive reads with the unparked sensor ON while waiting to retry a close
        uint8_t m_OpenConfirmCount {0};
        // The roof left the open limit since the current close started
        bool m_LeftOpenLimit {false};
        // A close failed, waiting for the roof to come back to rest at the open limit
        bool m_RecloseWaiting {false};
        uint8_t m_CloseRetriesLeft {0};
        // A motion timed out and the relay was released, pulse it again on the next read
        bool m_NudgePending {false};
        // A new pulse was already sent during the current opening
        bool m_OpenNudgeDone {false};
        std::chrono::steady_clock::time_point m_MotionStart;
        std::chrono::steady_clock::time_point m_ParkedTime;
        std::chrono::steady_clock::time_point m_RecloseDeadline;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * RELAYS_TAB = "Relays";
        static constexpr const char * SENSORS_TAB = "Sensors";
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {128};
        // Operatives
        static constexpr const uint8_t DRIVER_OPERATIVES {2};
        // Models
        static constexpr const uint8_t DRIVER_MODELS {4};
        // Sensor ON threshold
        static constexpr const uint8_t SENSOR_THRESHOLD {50};
        // Sensor Update Threshold
        static constexpr const uint8_t SENSOR_UPDATE_THRESHOLD {2};
        // Relay Update Threshold
        static constexpr const uint8_t RELAY_UPDATE_THRESHOLD {5};
        // Consecutive sensor reads required to trust a limit switch change
        static constexpr const uint8_t SENSOR_CONFIRM_READS {2};
        // After a park, a roof leaving the closed limit within this delay is treated as a
        // failed close (e.g. controller obstacle reversal), not as a manual opening.
        static constexpr const int RECLOSE_WINDOW_S {120};
        // Maximum time to wait for the roof to come back to the open limit before retrying
        static constexpr const int RECLOSE_WAIT_S {120};

};
