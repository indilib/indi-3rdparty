/*
    ScopeLink INDI driver

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/device.h"
#include "scopelink/faults.h"
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/transport.h"

#include <defaultdevice.h>
#include <indifocuserinterface.h>
#include <indipropertylight.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include <memory>
#include <string>
#include <vector>

namespace Connection
{
class Serial;
}

// Drivers conventionally reach the focuser interface through this short name. Declaring it here rather
// than relying on libindi to have done so keeps the driver building against releases that do not, and a
// second identical alias is harmless. The guard is for the case where a release spells it as a macro.
#ifndef FI
using FI = INDI::FocuserInterface;
#endif

/**
 * @brief One ScopeLink controller, published as one INDI device.
 *
 * The Windows driver has to be two COM servers - a focuser and a switch - that share a device object and
 * must be pointed at the same port by hand. INDI has no such constraint, so this is one driver process
 * owning one port and advertising every interface the connected hardware turns out to have:
 *
 *   - focuser, always
 *   - light box, always, driving the flat panel duty cycle
 *   - dust cap, once the front flap has a calibrated travel to move along
 *   - auxiliary, for the fans, the power outputs, the telemetry and the diagnostics
 *
 * Everything runs on libindi's event loop, which is single threaded. A transaction blocks that loop for
 * as long as it takes - at worst three attempts of a 100 ms receive timeout - so the polling period is
 * kept comfortably above that.
 */
class ScopeLink : public INDI::DefaultDevice, public INDI::FocuserInterface
{
    public:
        ScopeLink();
        virtual ~ScopeLink() override = default;

        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        bool Disconnect() override;
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

        // Focuser interface
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool AbortFocuser() override;
        bool SyncFocuser(uint32_t ticks) override;
        bool SetFocuserMaxPosition(uint32_t ticks) override;

    private:
        /** How close to an end stop the flap has to stop for the move to count as having arrived. */
        static constexpr int MinimumCapTolerance = 2;

        /** Most stored faults one property can show. The wire format cannot return more in one read. */
        static constexpr size_t MaxShownFaults = 8;

        /** Identifier of the configuration value holding a motor's calibrated end of travel. */
        static constexpr uint32_t FocuserMaximumPositionDid = 0x000f;
        static constexpr uint32_t FocuserSavedPositionDid   = 0x000e;
        static constexpr uint32_t FlapMaximumPositionDid    = 0x010f;
        static constexpr uint32_t FlapSavedPositionDid      = 0x010e;

        /** What the cover was last told to do, so that the poll knows when it has finished. */
        enum class CapTarget
        {
            None,
            Park,
            Unpark
        };

        // Connection and link handling ------------------------------------------------------------

        /**
         * @brief Identifies the controller once libindi has opened the port.
         * @return True when a supported ScopeLink answered.
         */
        bool handshake();

        /** @brief The body of the handshake, so that every failure path releases what it built. */
        bool identify();

        /** @brief Drops everything that depends on the port's file descriptor. */
        void releaseDevice();

        /** @brief Reports the link as lost and takes the device to a disconnected state. */
        void reportLinkLost(const std::string &reason);

        // Property construction -------------------------------------------------------------------

        void buildHardwareProperties();
        void defineHardwareProperties();
        void deleteHardwareProperties();
        void defineDeveloperProperties();
        void deleteDeveloperProperties();

        // Poll --------------------------------------------------------------------------------------

        void publishTelemetry(const scopelink::Status &status);
        void publishFocuser(const scopelink::Status &status);
        void publishCap(const scopelink::Status &status);
        void publishLightBox(const scopelink::Status &status);
        void publishFans(const scopelink::Status &status);
        void publishPower(const scopelink::Status &status);
        void publishCalibration(const scopelink::Status &status);

        // Cover ---------------------------------------------------------------------------------------

        IPState moveCap(CapTarget target);
        void adoptCapStateFromPosition(const scopelink::Status &status);

        // Parameters ----------------------------------------------------------------------------------

        /**
         * @brief One published property holding every configuration identifier of one subsystem.
         *
         * How many identifiers a group has depends on the hardware generation, so the properties are
         * built at connect time rather than declared with a fixed element count. Offering an element for
         * an identifier the controller does not hold would show a value that can never be read and, worse,
         * accept a write that goes nowhere.
         */
        struct ParameterGroup
        {
                scopelink::DidGroup group{ scopelink::DidGroup::Miscellaneous };
                std::unique_ptr<INDI::PropertyNumber> property;

                /** Identifier behind each element, in element order. */
                std::vector<uint32_t> identifiers;
        };

        void buildParameterGroups();
        void readAllParameters();
        bool applyParameterGroup(ParameterGroup &group, double values[], char *names[], int n);
        void publishParameterValues();
        void exportParameters();
        void importParameters();
        scopelink::Did *findDid(uint32_t id);

        /** @brief Property name a parameter group is published under. */
        static const char *parameterGroupProperty(scopelink::DidGroup group);

        // Diagnostics ----------------------------------------------------------------------------------

        void readFaults();
        void clearFaults();
        void readEepromStatistics();
        void publishLinkHealth();

        // Calibration ----------------------------------------------------------------------------------

        /** @brief Everything that differs between the motors the calibration page can drive. */
        struct MotorChannel
        {
                const char *name{ "" };
                int motorId{ 0 };
                uint32_t savedPositionDid{ 0 };
                uint32_t maximumPositionDid{ 0 };
        };

        MotorChannel selectedChannel() const;
        void calibrationJog(int steps);
        void calibrationMarkMinimum();
        void calibrationMarkMaximum();
        void calibrationReset();

        // Helpers ----------------------------------------------------------------------------------------

        /** @brief Reads a motor's calibrated end of travel out of the controller. */
        bool readMotorTravel(uint32_t maximumPositionDid, int &travel);

        /** @brief True when the driver is connected and the controller has been identified. */
        bool isReady() const;

        /**
         * @brief True when this unit has a front flap that can be driven to a known end of travel.
         *
         * A flap motor is not enough on its own: without a calibrated travel there is no position to open
         * to, so the cover is neither published nor claimed as a dust cap.
         */
        bool hasUsableFlap() const { return m_flapTravel > 0; }

        Connection::Serial *m_serialConnection{ nullptr };

        /**
         * The link the protocol layer runs on: a descriptor borrowed from the connection plugin, or a
         * simulated controller when the device is in simulation mode. Nothing above this line knows which.
         */
        std::unique_ptr<scopelink::ISerialTransport> m_transport;
        std::unique_ptr<scopelink::Protocol> m_protocol;
        std::unique_ptr<scopelink::Device> m_device;

        std::vector<scopelink::Did> m_catalogue;

        /** Focuser travel in controller steps, as calibrated. */
        int m_focuserTravel{ 0 };

        /** Flap travel in controller steps, as calibrated. Zero when there is no flap motor. */
        int m_flapTravel{ 0 };

        /** Client steps to controller steps. Configured, applied at connect. */
        int m_stepMultiplier{ 1 };

        CapTarget m_capTarget{ CapTarget::None };

        /** Where the focuser was last told to go, in client steps, so the poll can tell arrival from a stall. */
        double m_focusTarget{ 0 };

        /**
         * Polls to ignore before a motor that is neither moving nor at its target counts as stalled.
         *
         * The controller has been measured to report a motor as moving in the very status frame that
         * follows the command, but one poll of grace costs nothing and is the difference between a
         * slightly slow controller and a driver that reports a stall on every move.
         */
        int m_focusSettle{ 0 };
        int m_capSettle{ 0 };

        /**
         * Duty cycle the flat panel returns to when it is switched on. Kept here rather than read back
         * from the controller because the controller only knows the duty it is running, which is zero
         * while the panel is off - polling it back would erase the setting the user just typed.
         */
        int m_lightIntensity{ 0 };

        /** Number of consecutive polls that have failed, which is what decides when to give up. */
        int m_failedPolls{ 0 };

        // Properties ---------------------------------------------------------------------------------------

        INDI::PropertyText IdentificationTP{ 4 };

        INDI::PropertyNumber FocusTemperatureNP{ 1 };

        INDI::PropertySwitch CapParkSP{ 2 };
        INDI::PropertyNumber CapPositionNP{ 2 };

        INDI::PropertySwitch LightSP{ 2 };
        INDI::PropertyNumber LightIntensityNP{ 1 };

        INDI::PropertyNumber RailsNP{ 3 };
        INDI::PropertyNumber TemperatureNP{ 3 };
        INDI::PropertyNumber MotorLoadNP{ 2 };
        INDI::PropertyNumber ControllerNP{ 7 };

        INDI::PropertySwitch FanOverrideSP{ 2 };
        INDI::PropertySwitch FanStateSP{ 2 };
        INDI::PropertyNumber FanTargetNP{ 2 };

        INDI::PropertySwitch AuxPowerSP{ 2 };
        INDI::PropertyLight UsbHubLP{ 4 };

        INDI::PropertyNumber StepMultiplierNP{ 1 };
        INDI::PropertySwitch DeveloperSP{ 2 };

        /** Generation the simulated controller reports. Only consulted while simulation is on. */
        INDI::PropertySwitch SimulatedGenerationSP{ 2 };

        std::vector<ParameterGroup> m_parameterGroups;
        INDI::PropertySwitch ParametersActionSP{ 3 };
        INDI::PropertyText ParametersFileTP{ 1 };

        INDI::PropertyNumber FaultCountNP{ 2 };
        INDI::PropertySwitch DiagnosticActionSP{ 3 };
        INDI::PropertyText FaultStoreTP{ MaxShownFaults };
        INDI::PropertyNumber EepromNP{ 7 };
        INDI::PropertyText LinkHealthTP{ 1 };

        INDI::PropertySwitch CalibrationMotorSP{ 2 };
        INDI::PropertyNumber CalibrationStatusNP{ 4 };
        INDI::PropertyNumber CalibrationStepNP{ 1 };
        INDI::PropertySwitch CalibrationActionSP{ 6 };
};
