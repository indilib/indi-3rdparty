/*
    DragonFly Controller

    Copyright (C) 2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <indiinputinterface.h>
#include <indioutputinterface.h>
#include <defaultdevice.h>
#include <connectionplugins/connectiontcp.h>

class DragonFly : public INDI::DefaultDevice, public INDI::InputInterface, public INDI::OutputInterface
{
    public:
        DragonFly();
        virtual ~DragonFly() override = default;

        const char *getDefaultName() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;

    protected:
        void TimerHit() override;

        // Config
        virtual bool saveConfigItems(FILE * fp) override;

        /**
             * \brief Update all digital inputs
             * \return True if operation is successful, false otherwise
             */
        virtual bool UpdateDigitalInputs() override;

        /**
         * \brief Update all analog inputs
         * \return True if operation is successful, false otherwise
         */
        virtual bool UpdateAnalogInputs() override;

        /**
         * \brief Update all digital outputs
         * \return True if operation is successful, false otherwise
         * \note UpdateDigitalOutputs should either be called periodically in the child's TimerHit or custom timer function or when an
         * interrupt or trigger warrants updating the digital outputs. Only updated properties that had a change in status since the last
         * time this function was called should be sent to the clients to reduce unnecessary updates.
         * Polling or Event driven implemetation depends on the concrete class hardware capabilities.
         */
        virtual bool UpdateDigitalOutputs() override;

        /**
         * \brief Send command to output
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandOutput(uint32_t index, OutputState command) override;

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Set & Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool echo();

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

        // Firmware Version
        INDI::PropertyText FirmwareVersionTP {1};

        ///////////////////////////////////////////////////////////////////////
        /// Private Variables
        ///////////////////////////////////////////////////////////////////////
        uint32_t m_UpdateRelayCounter {0};
        uint32_t m_UpdateSensorCounter {0};
        Connection::TCP *tcpConnection {nullptr};
        int PortFD{-1};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
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

};
