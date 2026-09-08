/*
    ScopeLink INDI driver - simulated controller

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/device.h"
#include "scopelink/transport.h"
#include "scopelink/types.h"

#include <map>
#include <vector>

namespace scopelink
{

/**
 * @brief A controller that answers the wire protocol without any hardware behind it.
 *
 * It is not a model of the firmware. It answers every command the driver sends, with frame layouts taken
 * from the same measurements the driver's parsers are built on, and it moves its motors at a plausible
 * rate so that the driver's motion handling has something to follow.
 *
 * It lives in the core rather than in the simulator tool because two things need it: the tool, which
 * serves it on a pseudo terminal, and the driver's own simulation mode, which talks to it directly. A
 * second implementation for the driver would be a second thing to keep in step with the parsers.
 */
class SimulatedController
{
    public:
        /**
         * @param hardwareMajor Generation to imitate, 2 to 4. What it offers follows from that - how many
         *        motors it drives, how many downstream USB ports it reports, and which fault numbering it
         *        speaks.
         * @param interfaceMinor Interface version to report, 0 or 1
         *
         * The two are separate for the same reason the capability set keeps them apart: the same
         * generation 3 board runs both interface versions, and 1.1 changes the frames it sends without
         * changing anything about the board. Simulating only 1.0 is what let a driver that could not
         * connect to a 1.1 controller at all pass its whole test suite.
         *
         * Generation 4 exists only on interface 1.1 - it is the board the assignment was built for - so a
         * caller that asks for generation 4 on 1.0 gets a controller nobody makes. The tool that serves
         * this refuses that pair rather than serving it.
         */
        explicit SimulatedController(int hardwareMajor, int interfaceMinor = 0);

        /** @brief Builds the reply to one request frame, or an empty frame for a command with no reply. */
        Frame handle(const Frame &request);

    private:
        /** @brief One simulated motor, moving towards its target at a fixed rate. */
        struct Motor
        {
                int position{ 0 };
                int target{ 0 };
                unsigned load{ 0 };

                /** @brief Advances towards the target and reports the status byte the wire format uses. */
                uint8_t advance(long elapsedMs);
        };

        /** @brief One entry of the fault store, as both the status frame and the fault read report it. */
        struct StoredFault
        {
                uint16_t code{ 0 };
                uint8_t occurrences{ 1 };

                /** Still present now, as opposed to recorded and since cleared up. */
                bool active{ false };
        };

        static long now();
        void tick();

        static Frame frame(uint8_t command, const Frame &payload);
        static void put16(Frame &frame, unsigned value);
        static void put32(Frame &frame, unsigned value);

        Frame softwarePayload() const;
        Frame statusPayload() const;
        uint8_t flapState() const;
        Frame faultPayload() const;
        Frame dataIdentifier(const Frame &payload);
        Frame motorCommand(const Frame &payload);
        Frame functionCommand(const Frame &payload);
        Frame fanCommand(const Frame &payload);

        /**
         * @brief The assignment this controller holds in its configuration, worked out from its own
         *        identifiers.
         *
         * What a client reads back, and what it changes by writing those identifiers. It is not what the
         * controller is driving from - see @ref m_running.
         */
        MotorRoles storedRoles() const;

        /** @brief Drives one motor of a function, or reports why it will not move. */
        FunctionResponse driveMotor(int index, uint8_t subFunction, int position);

        /** @brief The travel a motor has been calibrated to, or zero when it has never been. */
        int travelOf(int index) const;

        int value(uint32_t did) const;
        size_t didLength(uint32_t did) const;
        static int defaultValue(uint32_t did);
        uint8_t activeFaultCount() const;

        int m_hardwareMajor;
        int m_interfaceMinor;

        /** Derived once, so that the frames served here cannot disagree with the ones the driver expects. */
        Capabilities m_capabilities;

        long m_lastTick{ 0 };

        /**
         * One entry per motor this generation drives, indexed the way the protocol numbers them.
         *
         * A list rather than a motor per job, because from interface 1.1 the job is a setting: what the
         * wire carries is the motors in a row, and which of them opens the flap is answered by the
         * identifiers this controller holds - see roles() - rather than by a mapping written out here.
         */
        std::vector<Motor> m_motors;

        /** The status byte each motor last reported, as the status frame carries it. */
        std::vector<uint8_t> m_motorStatus;

        /**
         * The assignment the running firmware is driving from: the stored one as it was when this
         * controller started, and again whenever it is reset.
         *
         * Real firmware reads its configuration at startup and drives from that copy, so an assignment
         * written while it runs is stored, reads back at once, and changes nothing about what the
         * controller does - a function command for something only the new assignment names is refused as
         * NotConfigured until it restarts. This used to be recomputed on every use, which made the
         * simulator the one controller in the world where a new assignment took effect immediately, and
         * let a driver that never told anybody to restart pass its whole test suite.
         */
        MotorRoles m_running;

        bool m_fanOverride[2]{ false, false };
        bool m_fanOn[2]{ false, false };
        bool m_power[2]{ false, false };
        int m_flatboxDuty{ 0 };

        std::map<uint32_t, int> m_dids;

        /** Wire length of each identifier this generation holds, taken from the catalogue at startup. */
        std::map<uint32_t, size_t> m_didLength;

        /**
         * One stored fault: the infrared sensor communication timeout, still active. It is the fault a
         * developer is most likely to meet on a unit whose sensor has failed, and it gives the fault store
         * and its freeze frame something to show.
         *
         * Written here as the FaultCode identity, which the constructor turns into the number this
         * generation sends for it. The two agree on every controller before generation 4 and part company
         * on that one, so a literal here would have been the wrong fault on exactly the controller this
         * translation exists for.
         */
        std::vector<StoredFault> m_faults{ StoredFault{ 4, 3, true } };
};

/**
 * @brief A transport with a SimulatedController on the far end instead of a port.
 *
 * Requests are reassembled exactly as the pseudo terminal tool reassembles them, so the driver's
 * simulation mode exercises the same framing path as a real link. A command the simulated generation does
 * not answer leaves the buffer empty, and the read that follows raises the same CommunicationError a
 * silent controller would - which is what makes capability discovery behave as it does on hardware.
 */
class SimulatedTransport : public ISerialTransport
{
    public:
        /**
         * @param hardwareMajor Generation the simulated controller reports, 2 or 3
         * @param interfaceMinor Interface version it reports, 0 or 1
         */
        explicit SimulatedTransport(int hardwareMajor, int interfaceMinor = 0);

        bool isOpen() const override;
        void close() override;
        void discardBuffers() override;
        void write(const Frame &data) override;
        Frame read(size_t count) override;
        bool reopen() override;
        std::string name() const override;

    private:
        SimulatedController m_controller;
        Frame m_request;
        Frame m_reply;
        bool m_open{ true };
};

} // namespace scopelink
