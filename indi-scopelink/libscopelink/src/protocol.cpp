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

#include "scopelink/protocol.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

namespace scopelink
{

/** @brief Formats a command identifier the way every message in this file refers to one. */
static std::string commandText(uint8_t command)
{
    char text[8];

    snprintf(text, sizeof(text), "0x%02X", command);

    return text;
}

/**
 * @brief Growable read-ahead buffer over a transport.
 *
 * Exists so that receiveFrame() can look at bytes it has already taken off the port while it hunts for
 * the preamble, and still pull the payload in one transfer once it knows how long the frame is.
 */
class FrameBuffer
{
    public:
        explicit FrameBuffer(ISerialTransport &transport) : m_transport(transport) {}

        /** @brief One buffered byte. Only valid below the count passed to the last require(). */
        uint8_t operator[](size_t index) const { return m_data[index]; }

        /**
         * @brief Reads from the transport until at least @p total bytes are buffered.
         * @throws CommunicationError The bytes did not arrive.
         */
        void require(size_t total)
        {
            if (total <= m_data.size())
                return;

            const size_t wanted = total - m_data.size();
            const Frame chunk   = m_transport.read(wanted);

            if (chunk.size() != wanted)
            {
                throw ProtocolError("The transport returned " + std::to_string(chunk.size()) + " of the "
                                    + std::to_string(wanted) + " bytes that were asked for.");
            }

            m_data.insert(m_data.end(), chunk.begin(), chunk.end());
        }

        /** @brief Copies a run of buffered bytes out. */
        Frame extract(size_t start, size_t length) const
        {
            return Frame(m_data.begin() + static_cast<long>(start), m_data.begin() + static_cast<long>(start + length));
        }

    private:
        ISerialTransport &m_transport;
        Frame m_data;
};

/**
 * @brief Decides whether a failure requires the port to be recycled rather than just retried.
 *
 * A timeout or a malformed frame means the device or the line misbehaved and the port is still usable.
 * Everything else - the port disappeared, the handle went stale, the write failed - needs a fresh one.
 */
static bool isPortLevelFailure(const std::exception &error)
{
    return (dynamic_cast<const TimeoutError *>(&error) == nullptr)
           && (dynamic_cast<const ProtocolError *>(&error) == nullptr);
}

Protocol::Protocol(ISerialTransport &transport) : m_transport(transport) {}

void Protocol::setLogger(Logger logger)
{
    m_logger = std::move(logger);
}

Frame Protocol::transact(uint8_t command, const Frame &payload, size_t expectedResponseLength)
{
    if (expectedResponseLength < HeaderLength)
        throw ProtocolError("A response frame is at least " + std::to_string(HeaderLength) + " bytes long.");

    return exchange(command, payload, expectedResponseLength, expectedResponseLength);
}

Frame Protocol::transactVariable(uint8_t command, const Frame &payload, size_t minimumResponseLength)
{
    if (minimumResponseLength < HeaderLength)
        throw ProtocolError("A response frame is at least " + std::to_string(HeaderLength) + " bytes long.");

    return exchange(command, payload, minimumResponseLength, 0);
}

Frame Protocol::buildRequest(uint8_t command, const Frame &payload)
{
    if (payload.size() > 255)
    {
        throw ProtocolError("Payload of " + std::to_string(payload.size())
                            + " bytes exceeds the 255 byte frame limit.");
    }

    Frame request;

    request.reserve(HeaderLength + payload.size());
    request.push_back(PreambleFirst);
    request.push_back(PreambleSecond);
    request.push_back(command);
    request.push_back(static_cast<uint8_t>(payload.size()));
    request.insert(request.end(), payload.begin(), payload.end());

    return request;
}

Frame Protocol::exchange(uint8_t command, const Frame &payload, size_t minimumResponseLength,
                         size_t exactResponseLength)
{
    const Frame request = buildRequest(command, payload);

    if (!m_transport.isOpen())
    {
        throw CommunicationError("Command " + commandText(command)
                                 + " cannot be sent because the ScopeLink link is not open.");
    }

    std::string lastError;

    for (int attempt = 1; attempt <= MaxAttempts; attempt++)
    {
        try
        {
            m_transport.discardBuffers();

            // Both directions are logged as hex, because a frame nobody can see is a frame nobody can
            // check. It is what a capture for tests/vectors is taken from, and what turns a support
            // report about a misread value into something answerable.
            log("wire", "-> " + toHex(request, request.size()));

            m_transport.write(request);

            Frame response = receiveFrame(command, exactResponseLength);

            log("wire", "<- " + toHex(response, response.size()));

            validateResponse(response, command, minimumResponseLength, exactResponseLength);

            m_transactionCount++;

            if (m_consecutiveFailures > 0)
            {
                log("health", "Link recovered after " + std::to_string(m_consecutiveFailures)
                                  + " consecutive failed transactions");
                m_consecutiveFailures = 0;
            }

            return response;
        }
        catch (const std::exception &error)
        {
            lastError = error.what();
            m_retryCount++;

            log("transact", "Command " + commandText(command) + " attempt " + std::to_string(attempt) + " of "
                                + std::to_string(MaxAttempts) + " failed: " + lastError);

            if (attempt >= MaxAttempts)
                break;

            // A plain timeout is usually a single lost frame, so retry on the open port first. Anything
            // else, or a second failure in a row, means the port itself needs recycling.
            if (isPortLevelFailure(error) || (attempt >= AttemptsBeforePortReopen))
            {
                if (m_transport.reopen())
                {
                    log("transact", "Recycled " + m_transport.name());
                }
                else
                {
                    // Nothing here can put the port back, so there is no point spending the remaining
                    // attempts on it. Reporting now gets the driver to a visible disconnect sooner.
                    log("transact", "The port cannot be recycled from here, abandoning the transaction");
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(RetryDelayMs));
        }
    }

    m_failureCount++;
    m_consecutiveFailures++;

    throw CommunicationError("No valid response to command " + commandText(command) + " after "
                             + std::to_string(MaxAttempts) + " attempts (" + std::to_string(m_consecutiveFailures)
                             + " consecutive failures). Last error: " + lastError);
}

Frame Protocol::receiveFrame(uint8_t command, size_t exactResponseLength)
{
    FrameBuffer buffer(m_transport);
    size_t start = 0;

    // The header read below is a single four byte transfer in the normal case and the preamble is at
    // offset zero, so this loop costs one comparison. It only starts stepping - one byte at a time, so
    // that a preamble straddling the end of what has been read is still found - when the stream is out of
    // step, which is the case that would otherwise lose the whole transaction.
    while (true)
    {
        buffer.require(start + HeaderLength);

        if ((buffer[start] == PreambleFirst) && (buffer[start + 1] == PreambleSecond))
            break;

        if (start >= MaxResyncBytes)
        {
            throw ProtocolError("No frame preamble in the " + std::to_string(start + HeaderLength)
                                + " bytes the controller sent in reply to command " + commandText(command) + ".");
        }

        start++;
    }

    size_t frameLength = HeaderLength + buffer[start + 3];

    if (exactResponseLength > MaxSelfDescribingFrameLength)
    {
        // The frame is longer than the length byte can count, so the caller's figure is the only one
        // available. In practice this is a fault store full of faults.
        log("receive", "Command " + commandText(command) + " expects " + std::to_string(exactResponseLength)
                           + " bytes, too long for the length byte to describe; reading the expected length");

        frameLength = exactResponseLength;
    }

    buffer.require(start + frameLength);

    if (start > 0)
    {
        log("receive", "Resynchronised on the preamble after " + std::to_string(start)
                           + " stray byte(s) before the reply to command " + commandText(command));
    }

    return buffer.extract(start, frameLength);
}

void Protocol::validateResponse(const Frame &response, uint8_t command, size_t minimumResponseLength,
                                size_t exactResponseLength) const
{
    if ((exactResponseLength > 0) && (response.size() != exactResponseLength))
    {
        throw ProtocolError("Command " + commandText(command) + " returned " + std::to_string(response.size())
                            + " bytes, expected " + std::to_string(exactResponseLength) + ".");
    }

    if (response.size() < minimumResponseLength)
    {
        throw ProtocolError("Command " + commandText(command) + " returned " + std::to_string(response.size())
                            + " bytes, at least " + std::to_string(minimumResponseLength) + " are needed.");
    }

    // The preamble is guaranteed by receiveFrame, which will not return a frame without one.
    if (response[2] != command)
    {
        throw ProtocolError("Command " + commandText(command) + " returned a frame for command "
                            + commandText(response[2]) + ".");
    }
}

std::string Protocol::healthSummary() const
{
    return "port: " + m_transport.name() + ", transactions: " + std::to_string(m_transactionCount)
           + ", retried attempts: " + std::to_string(m_retryCount) + ", failed transactions: "
           + std::to_string(m_failureCount) + ", consecutive failures: " + std::to_string(m_consecutiveFailures);
}

void Protocol::log(const char *scope, const std::string &message) const
{
    if (m_logger)
        m_logger(scope, message);
}

} // namespace scopelink
