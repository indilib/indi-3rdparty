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

#include "scopelink/transport.h"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace scopelink
{

/** @brief Milliseconds elapsed on a clock that does not step when the system time is adjusted. */
static int64_t monotonicMs()
{
    struct timespec now = {};

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (static_cast<int64_t>(now.tv_sec) * 1000) + (now.tv_nsec / 1000000);
}

FdSerialTransport::FdSerialTransport(int fd, int receiveTimeoutMs, std::string portName)
    : m_fd(fd), m_receiveTimeoutMs(receiveTimeoutMs), m_portName(std::move(portName))
{
}

void FdSerialTransport::setDescriptor(int fd)
{
    m_fd = fd;
}

bool FdSerialTransport::isOpen() const
{
    return m_fd >= 0;
}

void FdSerialTransport::close()
{
    // The descriptor belongs to whoever handed it over, so it is released rather than closed. A
    // PosixSerialTransport overrides this because there it really is ours.
    m_fd = -1;
}

void FdSerialTransport::discardBuffers()
{
    if (m_fd >= 0)
        tcflush(m_fd, TCIOFLUSH);
}

void FdSerialTransport::write(const Frame &data)
{
    if (m_fd < 0)
        throw CommunicationError("Cannot write to " + m_portName + " because it is not open.");

    size_t written = 0;

    while (written < data.size())
    {
        const ssize_t count = ::write(m_fd, data.data() + written, data.size() - written);

        if (count < 0)
        {
            if (errno == EINTR)
                continue;

            throw CommunicationError("Writing to " + m_portName + " failed: " + std::strerror(errno));
        }

        written += static_cast<size_t>(count);
    }
}

Frame FdSerialTransport::read(size_t count)
{
    if (m_fd < 0)
        throw CommunicationError("Cannot read from " + m_portName + " because it is not open.");

    Frame buffer(count);

    size_t filled          = 0;
    const int64_t deadline = monotonicMs() + m_receiveTimeoutMs;

    while (filled < count)
    {
        const int64_t remaining = deadline - monotonicMs();

        if (remaining <= 0)
        {
            throw TimeoutError("Timed out reading from " + m_portName + " after " + std::to_string(m_receiveTimeoutMs)
                               + " ms with " + std::to_string(filled) + " of " + std::to_string(count)
                               + " bytes received.");
        }

        struct pollfd waiting = {};

        waiting.fd     = m_fd;
        waiting.events = POLLIN;

        const int ready = poll(&waiting, 1, static_cast<int>(remaining));

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            throw CommunicationError("Waiting on " + m_portName + " failed: " + std::strerror(errno));
        }

        if (ready == 0)
            continue; // The deadline test at the top of the loop is what ends this.

        // A device that has been unplugged reports readable and then returns nothing, so a zero length
        // read is treated as the port having gone away rather than as an empty but healthy read.
        const ssize_t received = ::read(m_fd, buffer.data() + filled, count - filled);

        if (received < 0)
        {
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;

            throw CommunicationError("Reading from " + m_portName + " failed: " + std::strerror(errno));
        }

        if (received == 0)
            throw CommunicationError("The port " + m_portName
                                     + " reported end of file; the controller has been disconnected.");

        filled += static_cast<size_t>(received);
    }

    return buffer;
}

bool FdSerialTransport::reopen()
{
    // Somebody else owns this descriptor, so there is nothing here that can honestly be re-opened.
    return false;
}

std::string FdSerialTransport::name() const
{
    return m_portName;
}

PosixSerialTransport::PosixSerialTransport(std::string devicePath, int receiveTimeoutMs)
    : FdSerialTransport(-1, receiveTimeoutMs, devicePath), m_devicePath(std::move(devicePath))
{
}

PosixSerialTransport::~PosixSerialTransport()
{
    PosixSerialTransport::close();
}

void PosixSerialTransport::open()
{
    close();

    const int fd = ::open(m_devicePath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

    if (fd < 0)
        throw CommunicationError("Cannot open " + m_devicePath + ": " + std::strerror(errno));

    struct termios settings = {};

    if (tcgetattr(fd, &settings) != 0)
    {
        const std::string reason = std::strerror(errno);

        ::close(fd);
        throw CommunicationError("Cannot read the settings of " + m_devicePath + ": " + reason);
    }

    cfmakeraw(&settings);

    settings.c_cflag |= (CLOCAL | CREAD);
    settings.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
    settings.c_cc[VMIN]  = 0;
    settings.c_cc[VTIME] = 0;

    // The controller is a CDC-ACM device, so the line rate is not used by anything on the wire. It is
    // still set to a sane value because a port left at B0 is treated as a hangup by some drivers.
    cfsetispeed(&settings, B115200);
    cfsetospeed(&settings, B115200);

    if (tcsetattr(fd, TCSANOW, &settings) != 0)
    {
        const std::string reason = std::strerror(errno);

        ::close(fd);
        throw CommunicationError("Cannot configure " + m_devicePath + ": " + reason);
    }

    tcflush(fd, TCIOFLUSH);

    setDescriptor(fd);
}

void PosixSerialTransport::close()
{
    if (m_fd >= 0)
        ::close(m_fd);

    m_fd = -1;
}

bool PosixSerialTransport::reopen()
{
    try
    {
        open();
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

} // namespace scopelink
