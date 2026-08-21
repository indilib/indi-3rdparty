/*
    ScopeLink INDI driver - request / response layer

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

namespace scopelink
{

/**
 * @brief Request / response layer for the ScopeLink wire protocol.
 *
 * A request frame is <tt>0xAA 0x55 &lt;command&gt; &lt;payload length&gt; &lt;payload...&gt;</tt>. A
 * response repeats the preamble and echoes the command byte, which is verified on every transaction, so a
 * desynchronised or truncated frame is retried rather than silently parsed as data.
 *
 * Responses are read the same way: the four header bytes first, then exactly as many payload bytes as the
 * controller's own length byte asks for. The caller still says how long it expects the frame to be, but
 * that is a check rather than an instruction, so a response one byte short is reported immediately with
 * the stream still aligned on the next frame boundary.
 *
 * If the header does not start with the preamble the reader hunts forward for it rather than discarding
 * the transaction, which recovers a link that a partially consumed or unsolicited frame has knocked out
 * of step.
 *
 * Recovery escalates deliberately: a timeout is first retried on the open port, which is the cheap and
 * overwhelmingly most common case, and only a repeated failure or a port level error asks the transport
 * to recycle the port. A transport that does not own its port declines, and the failure is reported to
 * the caller - in the INDI driver that becomes a disconnect the user can see, rather than a driver
 * quietly reopening a device that is no longer there.
 *
 * This class is not thread safe. It is driven from libindi's event loop, which is single threaded, and
 * the locking the Windows driver needs exists only because COM calls arrive on arbitrary RPC threads.
 */
class Protocol
{
    public:
        /** First preamble byte. */
        static constexpr uint8_t PreambleFirst = 0xaa;

        /** Second preamble byte. */
        static constexpr uint8_t PreambleSecond = 0x55;

        /** Length of the fixed frame header - preamble, command, payload length. */
        static constexpr size_t HeaderLength = 4;

        /**
         * @brief Longest frame the single byte length field can describe.
         *
         * Every response the driver asks for is well inside this except one: the fault store read grows by
         * one record per stored fault. Measured, a record is 40 bytes on a generation 2 controller and 54
         * on a generation 3, so the length byte stops being able to describe the frame at 7 and 5 stored
         * faults respectively. The reader falls back to the caller's own length in that case.
         */
        static constexpr size_t MaxSelfDescribingFrameLength = HeaderLength + 255;

        /**
         * @param transport Byte transport to use, which the caller keeps ownership of
         */
        explicit Protocol(ISerialTransport &transport);

        /** @brief Installs a log sink. Pass an empty function to silence the layer again. */
        void setLogger(Logger logger);

        /**
         * @brief Performs a request / response transaction, retrying and recovering as required.
         * @param command Command identifier
         * @param payload Command payload, may be empty
         * @param expectedResponseLength Number of response bytes to read, header included
         * @return The validated response frame
         * @throws CommunicationError No valid response was obtained.
         */
        Frame transact(uint8_t command, const Frame &payload, size_t expectedResponseLength);

        /**
         * @brief Performs a transaction whose response length the controller decides.
         * @param command Command identifier
         * @param payload Command payload, may be empty
         * @param minimumResponseLength Shortest frame the caller can make sense of, header included
         * @return The validated response frame, as long as the controller said it was
         *
         * For the fault store, which is the one response whose size depends on what the controller has to
         * say. The alternative is to read the fault count from a status frame first and size the read from
         * that, which loses a record if a fault is logged between the two transactions.
         */
        Frame transactVariable(uint8_t command, const Frame &payload, size_t minimumResponseLength);

        /** @brief Builds a request frame. */
        static Frame buildRequest(uint8_t command, const Frame &payload);

        /** @brief Total number of successful transactions. */
        long transactionCount() const { return m_transactionCount; }

        /** @brief Total number of failed attempts, including those a retry recovered from. */
        long retryCount() const { return m_retryCount; }

        /** @brief Total number of transactions that failed after exhausting all retries. */
        long failureCount() const { return m_failureCount; }

        /** @brief Number of transactions that have failed back to back. Zero after any success. */
        int consecutiveFailures() const { return m_consecutiveFailures; }

        /** @brief One line summary of link health, for logs and support requests. */
        std::string healthSummary() const;

    private:
        /** Total number of attempts made for a single transaction. */
        static constexpr int MaxAttempts = 3;

        /** Pause between attempts, giving the controller time to finish transmitting a late frame. */
        static constexpr int RetryDelayMs = 20;

        /** Number of failed attempts after which the port itself is recycled. */
        static constexpr int AttemptsBeforePortReopen = 2;

        /**
         * How many stray bytes the reader will step over while hunting for the preamble before giving up.
         * Sized to let a whole stale frame of any length be skipped in one go. The hunt only continues
         * while bytes are actually arriving, so this is a bound on garbage rather than on time.
         */
        static constexpr size_t MaxResyncBytes = 2 * MaxSelfDescribingFrameLength;

        Frame exchange(uint8_t command, const Frame &payload, size_t minimumResponseLength, size_t exactResponseLength);
        Frame receiveFrame(uint8_t command, size_t exactResponseLength);
        void validateResponse(const Frame &response, uint8_t command, size_t minimumResponseLength,
                              size_t exactResponseLength) const;
        void log(const char *scope, const std::string &message) const;

        ISerialTransport &m_transport;
        Logger m_logger;

        long m_transactionCount{ 0 };
        long m_retryCount{ 0 };
        long m_failureCount{ 0 };
        int m_consecutiveFailures{ 0 };
};

} // namespace scopelink
