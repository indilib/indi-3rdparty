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
        /** @param hardwareMajor Generation to imitate, 2 or 3. What it offers follows from that. */
        explicit SimulatedController(int hardwareMajor);

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
        Frame faultPayload() const;
        Frame dataIdentifier(const Frame &payload);
        Frame motorCommand(const Frame &payload);
        Frame fanCommand(const Frame &payload);

        int value(uint32_t did) const;
        size_t didLength(uint32_t did) const;
        static int defaultValue(uint32_t did);
        uint8_t activeFaultCount() const;

        int m_hardwareMajor;
        long m_lastTick{ 0 };

        Motor m_focuser;
        Motor m_flap;
        uint8_t m_focuserStatus{ 2 };
        uint8_t m_flapStatus{ 2 };

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
        /** @param hardwareMajor Generation the simulated controller reports, 2 or 3. */
        explicit SimulatedTransport(int hardwareMajor);

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
