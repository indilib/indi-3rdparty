/*
    ScopeLink INDI driver - controller simulator

    Serves the ScopeLink wire protocol on a pseudo terminal, so that the driver can be developed and
    tested without a controller attached, and so that anything that speaks to a serial port - the command
    line tool, a client, a shell script - can be pointed at it.

    The controller itself is scopelink::SimulatedController in the core; this is the pseudo terminal
    around it. The driver's own simulation mode uses the same class without going near a port.

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/device.h"
#include "scopelink/simulator.h"
#include "scopelink/types.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

using scopelink::Frame;

int main(int argc, char *argv[])
{
    int hardwareMajor = 3;

    // The interface version is asked for separately from the generation because it is a separate thing:
    // the same generation 3 board runs 1.0 and 1.1, and 1.1 is the one whose frames are laid out
    // differently. Serving only 1.0 is what left the 1.1 layout untested against anything.
    //
    // Generation 4 is the exception, and only because no such board was ever built with the older
    // firmware: it comes up on 1.1 unless the caller says otherwise, and asking for 1.0 on it is refused
    // below rather than served.
    int interfaceMinor = -1;

    for (int index = 1; index < argc; index++)
    {
        if ((std::strcmp(argv[index], "--generation") == 0) && ((index + 1) < argc))
            hardwareMajor = std::atoi(argv[++index]);
        else if ((std::strcmp(argv[index], "--interface-minor") == 0) && ((index + 1) < argc))
            interfaceMinor = std::atoi(argv[++index]);
        else if (std::strcmp(argv[index], "--help") == 0)
        {
            printf("Usage: scopelink-simulator [--generation 2|3|4] [--interface-minor 0|1]\n"
                   "Serves the ScopeLink protocol on a pseudo terminal and prints its device path.\n"
                   "\n"
                   "The interface version defaults to 1.0, except on generation 4, which only exists on\n"
                   "1.1 - that is the board the motor assignment was built for.\n");
            return 0;
        }
    }

    // Still at the sentinel means the caller did not name one.
    if (interfaceMinor < 0)
        interfaceMinor = (hardwareMajor > 3) ? 1 : 0;

    if (interfaceMinor > 1)
    {
        fprintf(stderr, "Interface version 1.%d is not one this simulator serves; use 0 or 1.\n", interfaceMinor);
        return 1;
    }

    // Refused rather than served, because the pair describes no controller anybody can buy: a generation 4
    // board has three motors and the assignment that says what they drive, and 1.0 is the firmware that
    // has neither. Serving it would produce a 65 byte frame nothing sends and prove nothing about either.
    if ((hardwareMajor > 3) && (interfaceMinor < 1))
    {
        fprintf(stderr, "Generation 4 only exists on interface 1.1; there is no 1.0 firmware for it.\n");
        return 1;
    }

    if (!scopelink::Capabilities::isSupported(hardwareMajor))
    {
        fprintf(stderr, "Hardware generation %d is outside the supported range of %d to %d.\n", hardwareMajor,
                scopelink::Capabilities::MinimumSupportedHardwareMajor,
                scopelink::Capabilities::MaximumSupportedHardwareMajor);
        return 1;
    }

    const int master = posix_openpt(O_RDWR | O_NOCTTY);

    if ((master < 0) || (grantpt(master) != 0) || (unlockpt(master) != 0))
    {
        fprintf(stderr, "Cannot create a pseudo terminal: %s\n", std::strerror(errno));
        return 1;
    }

    struct termios settings = {};

    if (tcgetattr(master, &settings) == 0)
    {
        cfmakeraw(&settings);
        tcsetattr(master, TCSANOW, &settings);
    }

    printf("ScopeLink simulator, hardware generation %d on interface 1.%d\n", hardwareMajor, interfaceMinor);
    printf("Port: %s\n", ptsname(master));
    fflush(stdout);

    scopelink::SimulatedController controller(hardwareMajor, interfaceMinor);
    Frame pending;

    while (true)
    {
        struct pollfd waiting = {};

        waiting.fd     = master;
        waiting.events = POLLIN;

        if (poll(&waiting, 1, 1000) <= 0)
            continue;

        uint8_t chunk[256];
        const ssize_t received = read(master, chunk, sizeof(chunk));

        if (received <= 0)
            continue;

        pending.insert(pending.end(), chunk, chunk + received);

        // Requests are framed exactly like responses, so the same length byte drives reassembly.
        while (pending.size() >= 4)
        {
            if ((pending[0] != 0xaa) || (pending[1] != 0x55))
            {
                pending.erase(pending.begin());
                continue;
            }

            const size_t length = 4 + pending[3];

            if (pending.size() < length)
                break;

            const Frame request(pending.begin(), pending.begin() + static_cast<long>(length));

            pending.erase(pending.begin(), pending.begin() + static_cast<long>(length));

            const Frame reply = controller.handle(request);

            if (!reply.empty())
            {
                if (write(master, reply.data(), reply.size()) < 0)
                    fprintf(stderr, "Write failed: %s\n", std::strerror(errno));
            }
        }
    }

    return 0;
}
