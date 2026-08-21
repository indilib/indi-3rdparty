/*
    ScopeLink INDI driver - byte transports

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#pragma once

#include "scopelink/types.h"

#include <string>

namespace scopelink
{

/**
 * @brief The byte level link the protocol layer sits on.
 *
 * Two implementations exist and they differ in who owns the file descriptor, which is the only thing the
 * protocol layer needs to know about a port: FdSerialTransport borrows one from libindi's connection
 * plugin, PosixSerialTransport opens its own. Everything else - framing, retries, validation - is written
 * once against this interface.
 */
class ISerialTransport
{
    public:
        virtual ~ISerialTransport() = default;

        /** @brief True while the transport can carry bytes. */
        virtual bool isOpen() const = 0;

        /** @brief Closes the link. Must tolerate being called on an already closed transport. */
        virtual void close() = 0;

        /** @brief Discards anything queued in either direction. */
        virtual void discardBuffers() = 0;

        /**
         * @brief Transmits the whole buffer.
         * @throws CommunicationError The bytes could not be handed to the port.
         */
        virtual void write(const Frame &data) = 0;

        /**
         * @brief Reads exactly @p count bytes.
         * @throws CommunicationError The bytes did not arrive within the receive timeout.
         */
        virtual Frame read(size_t count) = 0;

        /**
         * @brief Closes and re-opens the underlying port.
         * @return True when the transport is usable again.
         *
         * A transport that does not own its port cannot do this and says so by returning false, which the
         * protocol layer reports to its caller as a link failure rather than retrying into a port that is
         * never going to come back on its own.
         */
        virtual bool reopen() = 0;

        /** @brief Human readable name of the port, for logs and error text. */
        virtual std::string name() const = 0;
};

/**
 * @brief A transport over a file descriptor somebody else owns.
 *
 * This is what the INDI driver uses. libindi's serial connection plugin opens the port, applies the
 * user's baud rate and hands over the descriptor; closing it is the plugin's job, and reopening it means
 * going all the way back through the connection state machine, which is a driver level decision rather
 * than something the protocol layer should do behind the user's back.
 */
class FdSerialTransport : public ISerialTransport
{
    public:
        /**
         * @param fd Open file descriptor, or -1 for a transport that starts out closed
         * @param receiveTimeoutMs How long a single read may wait for its bytes
         * @param portName Name to use in log messages
         */
        FdSerialTransport(int fd, int receiveTimeoutMs, std::string portName);

        /** @brief Adopts a new descriptor, for instance after the driver has reconnected. */
        void setDescriptor(int fd);

        bool isOpen() const override;
        void close() override;
        void discardBuffers() override;
        void write(const Frame &data) override;
        Frame read(size_t count) override;
        bool reopen() override;
        std::string name() const override;

    protected:
        int m_fd{ -1 };
        int m_receiveTimeoutMs{ 100 };
        std::string m_portName;
};

/**
 * @brief A transport that opens a serial port by path and owns it.
 *
 * Used by the command line tool and the tests. Because it owns the descriptor it can honour reopen(),
 * which is what recovers a link whose port handle has gone stale without involving the user.
 */
class PosixSerialTransport : public FdSerialTransport
{
    public:
        /**
         * @param devicePath Port to open, for example /dev/ttyACM0
         * @param receiveTimeoutMs How long a single read may wait for its bytes
         */
        PosixSerialTransport(std::string devicePath, int receiveTimeoutMs);
        ~PosixSerialTransport() override;

        /**
         * @brief Opens the port and puts it into raw mode.
         * @throws CommunicationError The port could not be opened or configured.
         */
        void open();

        void close() override;
        bool reopen() override;

    private:
        std::string m_devicePath;
};

} // namespace scopelink
