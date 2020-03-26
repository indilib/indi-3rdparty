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
#include <indidome.h>

using DD = INDI::DefaultDevice;

class Relay
{
    public:
        explicit Relay(uint8_t id, const std::string &device, const std::string &group);
        ~Relay() = default;

        void define(DD *parent);
        void remove(DD *parent);
        void sync(IPState state);
        bool update(ISState *states, char *names[], int n);
        const std::string &name() const;
        bool isEnabled() const;
        void setEnabled(bool enabled);

    private:

        uint8_t m_ID;
        std::string m_Name;

        ISwitchVectorProperty RelaySP;
        ISwitch RelayS[2];
};

class DragonFlyDome : public INDI::Dome
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

    protected:
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
        bool setParam(const std::string &param, uint32_t value);
        bool getParam(const std::string &param, uint32_t &value);
        bool setSpeedRange(uint32_t min, uint32_t max);
        bool syncSettings();
        bool echo();

        ///////////////////////////////////////////////////////////////////////////////
        /// Relays
        ///////////////////////////////////////////////////////////////////////////////
        bool setRelayEnabled(uint8_t id, bool enabled);
        bool openRoof();
        bool closeRoof();
        bool updateRelays();

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
        ISwitchVectorProperty PerPortSP;
        ISwitch PerPortS[3];
        enum { PORT_MAIN, PORT_EXP, PORT_THIRD };

        // Motor wiring
        ISwitchVectorProperty WiringSP;
        ISwitch WiringS[4];
        enum { WIRING_LUNATICO_NORMAL, WIRING_LUNATICO_REVERSED, WIRING_RFMOONLITE_NORMAL, WIRING_RFMOONLITE_REVERSED };

        // Halfstep
        ISwitchVectorProperty HalfStepSP;
        ISwitch HalfStepS[2];

        // Motor Mode
        ISwitchVectorProperty MotorTypeSP;
        ISwitch MotorTypeS[4];
        enum { MOTOR_UNIPOLAR, MOTOR_BIPOLAR, MOTOR_DC, MOTOR_STEPDIR };

        // Roof Control Relays
        INumberVectorProperty DomeRelayNP;
        INumber DomeRelayN[2];
        enum
        {
            RELAY_OPEN,
            RELAY_CLOSE,
        };

        // All Relays
        std::vector<std::unique_ptr<Relay>> Relays;

        // Roof Control Sensors
        INumberVectorProperty DomeSensorNP;
        INumber DomeSensorN[4];
        enum
        {
            SENSOR_OPENED,
            SENSOR_CLOSED,
            SENSOR_UNPARKED,
            SENSOR_PARKED,
        };

        // All Sensors
        INumberVectorProperty SensorNP;
        INumber SensorN[8];

        // Motor Settings
        INumberVectorProperty SettingNP;
        INumber SettingN[5];
        enum { PARAM_MIN_SPEED, PARAM_MAX_SPEED, PARAM_MIN_LIMIT, PARAM_MAX_LIMIT, PARAM_STEPS_DEGREE };

        // Firmware Version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;

        ///////////////////////////////////////////////////////////////////////
        /// Private Variables
        ///////////////////////////////////////////////////////////////////////
        uint32_t m_UpdateRelayCounter {0};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * SETTINGS_TAB = "Settings";
        static constexpr const char * MOTOR_TAB = "Motor";
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

};
