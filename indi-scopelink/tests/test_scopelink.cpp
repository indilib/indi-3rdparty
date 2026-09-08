/*
    ScopeLink INDI driver - unit tests for the protocol core

    These run without libindi and without hardware. The frames below are hand built to the layouts the
    parsers are documented against, which makes them a check that the C++ port agrees with the Windows
    driver rather than merely that it agrees with itself.

    Captures from real controllers belong here too - see tests/vectors/README.md. A recorded frame that
    both this driver and the Windows one decode identically is the only thing that keeps two
    implementations of the same wire format honest.

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/device.h"
#include "scopelink/faults.h"
#include "scopelink/firmware.h"
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/roles.h"
#include "scopelink/simulator.h"
#include "scopelink/transport.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using scopelink::Frame;

namespace
{

int g_failures = 0;
int g_checks   = 0;

void check(bool condition, const char *expression, const char *file, int line)
{
    g_checks++;

    if (condition)
        return;

    g_failures++;
    printf("  FAIL %s:%d  %s\n", file, line, expression);
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

/** @brief Runs @p body and reports whether it threw the expected exception type. */
template <typename Exception>
bool throws(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const Exception &)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }

    return false;
}

/**
 * @brief A transport with a script instead of a port.
 *
 * Responses are queued in the order they are to be returned; an empty queue behaves like a silent
 * controller and raises a timeout, which is what drives the retry tests.
 */
class MockTransport : public scopelink::ISerialTransport
{
    public:
        std::deque<Frame> responses;
        std::vector<Frame> writes;
        int reopenCount{ 0 };
        bool reopenSucceeds{ true };
        bool open{ true };

        bool isOpen() const override { return open; }
        void close() override { open = false; }
        void discardBuffers() override {}

        void write(const Frame &data) override { writes.push_back(data); }

        Frame read(size_t count) override
        {
            if (m_pending.empty())
            {
                if (responses.empty())
                    throw scopelink::TimeoutError("No scripted response.");

                m_pending = responses.front();
                responses.pop_front();
            }

            if (m_pending.size() < count)
                throw scopelink::TimeoutError("Scripted response is shorter than the read.");

            Frame chunk(m_pending.begin(), m_pending.begin() + static_cast<long>(count));

            m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<long>(count));

            return chunk;
        }

        bool reopen() override
        {
            reopenCount++;

            // A successful reopen puts the port back, which is what the firmware update depends on: it
            // closes the link every time the controller leaves the bus and asks for a new one when it
            // comes back. A failed one leaves the transport exactly as it was.
            if (reopenSucceeds)
                open = true;

            return reopenSucceeds;
        }

        void setReceiveTimeout(int receiveTimeoutMs) override { timeouts.push_back(receiveTimeoutMs); }

        /** Every receive timeout the caller has asked for, in order. */
        std::vector<int> timeouts;

        std::string name() const override { return "mock"; }

    private:
        Frame m_pending;
};

/** @brief Builds a well formed response frame. */
Frame reply(uint8_t command, const Frame &payload)
{
    Frame frame{ 0xaa, 0x55, command, static_cast<uint8_t>(payload.size()) };

    frame.insert(frame.end(), payload.begin(), payload.end());

    return frame;
}

void put16(Frame &frame, unsigned value)
{
    frame.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    frame.push_back(static_cast<uint8_t>(value & 0xff));
}

void put32(Frame &frame, unsigned value)
{
    frame.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
    frame.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    frame.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    frame.push_back(static_cast<uint8_t>(value & 0xff));
}

/**
 * @brief Builds a status payload with recognisable values at every documented offset.
 *
 * Written independently of the driver's parser and of the simulator, so that agreement between the three
 * means something.
 *
 * The interface version is a second axis and not a third generation: the same board sends either of
 * these, and what changes is that 1.1 drops the four digital inputs out of the middle and puts the flap
 * state on the end. Every value before the inputs is identical in both, which is what makes a parser that
 * reads the tail at fixed offsets pass on one and quietly misread the other.
 */
Frame statusPayload(int hardwareMajor, int interfaceMinor = 0)
{
    Frame payload;

    put16(payload, 12345);  //  4 supply
    put16(payload, 4990);   //  6 IR sensor supply
    put16(payload, 11000);  //  8 fan A
    put16(payload, 10500);  // 10 fan B
    put16(payload, 0xffce); // 12 controller temperature, -50 as a signed 16 bit value
    put16(payload, 3300);   // 14 controller supply
    put16(payload, 0);      // 16 reserved

    payload.push_back(1); // 18 fan A override enabled
    payload.push_back(0); // 19 fan B override enabled
    payload.push_back(1); // 20 fan A commanded on
    payload.push_back(0); // 21 fan B commanded off

    put16(payload, 25);    // 22 fan A target, 0.5 K
    put16(payload, 100);   // 24 fan B target, 2.0 K
    put16(payload, 14650); // 26 ambient, 20.0 C
    put16(payload, 14400); // 28 mirror, 15.0 C

    payload.push_back(1);  // 30 motor 1 status, counter clockwise
    payload.push_back(42); // 31 motor 1 load
    put32(payload, 33000); // 32 motor 1 position

    payload.push_back(2);  // 36 motor 2 status, halted
    payload.push_back(17); // 37 motor 2 load
    put32(payload, 6000);  // 38 motor 2 position

    // The third motor, which generation 4 alone has. It pushes the whole of the tail six bytes further
    // along - the one difference between the two frame layouts that no interface version produces.
    if (hardwareMajor > 3)
    {
        payload.push_back(0);  // motor 3 status, clockwise
        payload.push_back(63); // motor 3 load
        put32(payload, 125);   // motor 3 position
    }

    payload.push_back(1); // aux output 1
    payload.push_back(0); // aux output 2

    // The four digital inputs, dropped by interface 1.1. Everything below shifts four bytes forward
    // without them, so a reader that has not noticed finds the flat box duty in the last of them.
    if (interfaceMinor < 1)
        put32(payload, 0); // 44 reserved

    payload.push_back(60); // 48 flat box duty
    payload.push_back(11); // 49
    payload.push_back(22); // 50
    payload.push_back(33); // 51
    payload.push_back(2);  // 52 stored faults
    payload.push_back(1);  // 53 active faults
    put16(payload, 7);     // 54 I2C errors

    // Generation 3 reports two ports and a failure flag for each; generation 4 reports six ports and no
    // failure flags, having gained four ports in the two bytes the flags used to take.
    if (hardwareMajor > 3)
    {
        payload.push_back(1); // port 1 powered
        payload.push_back(0); // port 2
        payload.push_back(1); // port 3
        payload.push_back(1); // port 4
        payload.push_back(0); // port 5
        payload.push_back(1); // port 6
    }
    else if (hardwareMajor > 2)
    {
        payload.push_back(1); // port 1 powered
        payload.push_back(0); // port 2 not powered
        payload.push_back(0); // port 1 no fault
        payload.push_back(1); // port 2 faulted
    }

    // The flap taken as a whole, which only a controller with configurable motor roles reports.
    if (interfaceMinor >= 1)
        payload.push_back(1); // opening

    return payload;
}

scopelink::Capabilities capabilitiesFor(int hardwareMajor, int temperatureSensorFitted = -1, int interfaceMajor = 1,
                                        int interfaceMinor = 0)
{
    scopelink::Identification identification;

    identification.hardwareMajor  = hardwareMajor;
    identification.interfaceMajor = interfaceMajor;
    identification.interfaceMinor = interfaceMinor;

    return scopelink::Capabilities::of(identification, temperatureSensorFitted);
}

// ---------------------------------------------------------------------------------------------------

void testByteOrder()
{
    const Frame data = { 0x80, 0x00, 0xff, 0xfe, 0x12, 0x34 };

    CHECK(scopelink::byte_order::toUInt(data, 0, 1) == 0x80u);
    CHECK(scopelink::byte_order::toUInt(data, 0, 2) == 0x8000u);
    CHECK(scopelink::byte_order::toUInt(data, 2, 2) == 0xfffeu);
    CHECK(scopelink::byte_order::toUInt(data, 2, 4) == 0xfffe1234u);

    CHECK(scopelink::byte_order::toInt(data, 0, 1) == -128);
    CHECK(scopelink::byte_order::toInt(data, 0, 2) == -32768);
    CHECK(scopelink::byte_order::toInt(data, 2, 2) == -2);
    CHECK(scopelink::byte_order::toInt(data, 4, 2) == 0x1234);
    CHECK(scopelink::byte_order::toInt(data, 2, 4) == static_cast<int32_t>(0xfffe1234));

    CHECK(scopelink::byte_order::toBool(data, 1) == false);
    CHECK(scopelink::byte_order::toBool(data, 2) == true);

    // A field that runs off the end names the offset rather than reading past the buffer.
    CHECK(throws<scopelink::ProtocolError>([&] { scopelink::byte_order::toUInt(data, 5, 2); }));
    CHECK(throws<scopelink::ProtocolError>([&] { scopelink::byte_order::toUInt(data, 0, 5); }));
}

void testCapabilities()
{
    const scopelink::Capabilities second = capabilitiesFor(2);
    const scopelink::Capabilities third  = capabilitiesFor(3);

    CHECK(second.statusFrameLength == 56);
    CHECK(third.statusFrameLength == 60);

    CHECK(second.dtcSnapshotLength == 34);
    CHECK(third.dtcSnapshotLength == 48);

    CHECK(!second.hasUsbHub);
    CHECK(third.hasUsbHub);
    CHECK(third.hasSmartSwitchDiagnostics);
    CHECK(!second.hasSmartSwitchDiagnostics);

    // Two axes that have to be read separately. The same generation 3 board runs both interface versions,
    // and reflashing it changes what it can be asked without changing what it is built from.
    CHECK(!capabilitiesFor(3, -1, 1, 0).hasConfigurableMotorRoles);
    CHECK(capabilitiesFor(3, -1, 1, 1).hasConfigurableMotorRoles);
    CHECK(capabilitiesFor(2, -1, 1, 1).hasConfigurableMotorRoles);
    CHECK(capabilitiesFor(3, -1, 2, 0).hasConfigurableMotorRoles);

    // The third motor is a board, not a firmware. A generation 3 controller reflashed to interface 1.1
    // holds the motor assignment and still has two motors to assign, which is the pair that stops these
    // two questions from being folded into one.
    CHECK(!third.hasThirdMotor);
    CHECK(!capabilitiesFor(3, -1, 1, 1).hasThirdMotor);
    CHECK(capabilitiesFor(4, -1, 1, 1).hasThirdMotor);

    CHECK(second.motorCount == 2);
    CHECK(third.motorCount == 2);

    // Interface 1.1 is a length of its own, belonging to no generation. It loses the four digital input
    // bytes out of the middle of both frames and gains one flap state byte on the end of the status
    // frame: 60 - 4 + 1 = 57, and 48 - 4 = 44.
    //
    // Reading the lengths off the generation number alone is what this replaced, and it did not fail
    // softly: Status::parse refuses a frame whose length is not the expected one, and open() takes its
    // first sample before it returns, so a 1.1 controller could not be connected to at all.
    const scopelink::Capabilities roles = capabilitiesFor(3, -1, 1, 1);

    CHECK(roles.statusFrameLength == 57);
    CHECK(roles.dtcSnapshotLength == 44);

    CHECK(!roles.hasDigitalInputs);
    CHECK(third.hasDigitalInputs);
    CHECK(second.hasDigitalInputs);

    // Everything else about the board is unchanged by reflashing it, which is the point of keeping the
    // two axes apart.
    CHECK(roles.hardwareMajor == third.hardwareMajor);
    CHECK(roles.motorCount == third.motorCount);
    CHECK(roles.hasUsbHub == third.hasUsbHub);
    CHECK(roles.hasSmartSwitchDiagnostics == third.hasSmartSwitchDiagnostics);

    CHECK(second.usbDownstreamPortCount == 0);
    CHECK(third.usbDownstreamPortCount == 2);
    CHECK(!second.hasUsbPowerFailureReporting);
    CHECK(third.hasUsbPowerFailureReporting);

    // Generation 4: a third motor, six downstream ports and no failure flags, on top of everything
    // interface 1.1 already changed. The status frame grows by the six bytes of the motor and by four
    // ports, and loses the two failure flags: 57 + 6 + 4 - 2 = 65.
    const scopelink::Capabilities fourth = capabilitiesFor(4, -1, 1, 1);

    CHECK(fourth.statusFrameLength == 65);
    CHECK(fourth.motorCount == 3);
    CHECK(fourth.usbDownstreamPortCount == 6);
    CHECK(!fourth.hasUsbPowerFailureReporting);
    CHECK(!fourth.hasDigitalInputs);
    CHECK(fourth.hasConfigurableMotorRoles);
    CHECK(fourth.hasUsbHub);
    CHECK(fourth.hasSmartSwitchDiagnostics);

    // The sharp one. Generation 4 stores 48 freeze frame bytes and so does a generation 3 controller on
    // interface 1.0, and they hold different fields in them: generation 4 spent the four digital inputs
    // and the two USB failure flags on a third motor and four more ports, which happens to come out even.
    // So a length check cannot tell those two apart, and nothing but the field list is going to.
    CHECK(fourth.dtcSnapshotLength == 48);
    CHECK(third.dtcSnapshotLength == 48);

    CHECK(!second.hasThirdMotor);
    CHECK(second.hasSecondMotor);
    CHECK(second.hasPowerSwitches);

    // The sensor is assumed fitted when the controller does not answer the identifier, and on any
    // generation that does not hold it at all.
    CHECK(capabilitiesFor(2, 0).hasTemperatureSensor);
    CHECK(capabilitiesFor(3, -1).hasTemperatureSensor);
    CHECK(capabilitiesFor(3, 1).hasTemperatureSensor);
    CHECK(!capabilitiesFor(3, 0).hasTemperatureSensor);

    // Either side of the supported range is refused rather than decoded with a neighbouring generation's
    // offsets, which would produce plausible readings from the wrong bytes.
    scopelink::Identification tooOld;
    scopelink::Identification tooNew;

    tooOld.hardwareMajor = scopelink::Capabilities::MinimumSupportedHardwareMajor - 1;
    tooNew.hardwareMajor = scopelink::Capabilities::MaximumSupportedHardwareMajor + 1;

    CHECK(throws<scopelink::UnsupportedDeviceError>([&] { scopelink::Capabilities::of(tooOld); }));
    CHECK(throws<scopelink::UnsupportedDeviceError>([&] { scopelink::Capabilities::of(tooNew); }));
}

void testStatusParsing()
{
    // The four controllers that exist as far as the frame layout is concerned. The interface version is
    // carried alongside the generation rather than folded into it because it is a separate axis: the
    // third of these is the same board as the second, reflashed, and the fourth is a different board on
    // the same firmware as the third.
    const int layouts[][2] = { { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 1 } };

    for (const auto &layout : layouts)
    {
        const int generation     = layout[0];
        const int interfaceMinor = layout[1];

        const scopelink::Capabilities capabilities = capabilitiesFor(generation, -1, 1, interfaceMinor);
        const Frame frame = reply(scopelink::command::Status, statusPayload(generation, interfaceMinor));

        CHECK(frame.size() == capabilities.statusFrameLength);

        const scopelink::Status status = scopelink::Status::parse(frame, capabilities);

        CHECK(status.supplyVoltage == 12345);
        CHECK(status.sensorSupplyVoltage == 4990);
        CHECK(status.fanAVoltage == 11000);
        CHECK(status.controllerTemperature == -50);
        CHECK(status.controllerSupplyVoltage == 3300);

        CHECK(status.fanAManualOverrideEnabled);
        CHECK(!status.fanBManualOverrideEnabled);
        CHECK(status.fanAManualOverrideState);
        CHECK(status.fanATargetDT == 25);
        CHECK(status.fanBTargetDT == 100);

        CHECK(status.ambientTemperatureValid);
        CHECK(status.mirrorTemperatureValid);
        CHECK(std::abs(status.ambientTemperatureCelsius() - 20.0) < 0.001);
        CHECK(std::abs(status.mirrorTemperatureCelsius() - 15.0) < 0.001);

        CHECK(status.motorCount() == ((generation > 3) ? 3 : 2));

        CHECK(status.motor(0).moving);
        CHECK(status.motor(0).direction == scopelink::MotorDirection::CounterClockwise);
        CHECK(status.motor(0).load == 42);
        CHECK(status.motor(0).position == 33000);

        CHECK(status.flatboxDuty == 60);
        CHECK(status.cpuLoad == 11);
        CHECK(status.peakCpuLoad == 22);
        CHECK(status.stackUsage == 33);
        CHECK(status.storedFaultCount == 2);
        CHECK(status.activeFaultCount == 1);
        CHECK(status.i2cErrorCounter == 7);

        CHECK(!status.motor(1).moving);
        CHECK(status.motor(1).direction == scopelink::MotorDirection::Halted);
        CHECK(status.motor(1).load == 17);
        CHECK(status.motor(1).position == 6000);

        // The third motor, and with it the proof that everything behind it was found six bytes further
        // along: the power switches, the tail and the flap state below are all read against the same
        // offset this motor moved.
        if (generation > 3)
        {
            CHECK(status.motor(2).moving);
            CHECK(status.motor(2).direction == scopelink::MotorDirection::Clockwise);
            CHECK(status.motor(2).load == 63);
            CHECK(status.motor(2).position == 125);
        }

        // A motor this controller does not have is answered rather than refused, with a reading that says
        // stopped at zero - which is something a caller polling twice a second can act on.
        CHECK(!status.motor(status.motorCount()).moving);
        CHECK(status.motor(status.motorCount()).position == 0);
        CHECK(status.motor(-1).position == 0);

        CHECK(status.powerSwitch1State);
        CHECK(!status.powerSwitch2State);

        CHECK(status.usbDownstreamPortCount() == ((generation > 3) ? 6 : ((generation > 2) ? 2 : 0)));

        if (generation == 3)
        {
            CHECK(status.usbPowerActive(0));
            CHECK(!status.usbPowerActive(1));
            CHECK(!status.usb1PowerFailure);
            CHECK(status.usb2PowerFailure);
        }

        if (generation > 3)
        {
            // Six ports, of which the fourth and the sixth are the ones a reader that stopped at two
            // would never have looked at.
            CHECK(status.usbPowerActive(0));
            CHECK(!status.usbPowerActive(1));
            CHECK(status.usbPowerActive(2));
            CHECK(status.usbPowerActive(3));
            CHECK(!status.usbPowerActive(4));
            CHECK(status.usbPowerActive(5));

            // No failure flags on this controller, so they stay as they were rather than picking up the
            // first two bytes of something else.
            CHECK(!status.usb1PowerFailure);
            CHECK(!status.usb2PowerFailure);
        }

        // A port this hub does not have is answered false rather than read off the end of the list.
        CHECK(!status.usbPowerActive(status.usbDownstreamPortCount()));

        // The flap as a whole, which only a controller with configurable motor roles reports. Before that
        // it is read off the flap motor, and saying so is not the same as saying there is no flap.
        CHECK(status.flapState
              == ((interfaceMinor >= 1) ? scopelink::FlapState::Opening : scopelink::FlapState::Unknown));
    }

    // A frame of the wrong length for the generation is refused rather than decoded with the offsets of
    // whichever generation it happens to be long enough for.
    CHECK(throws<scopelink::ProtocolError>(
        [] { scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(2)), capabilitiesFor(3)); }));

    // The two interface versions of one generation, each refused by the other's capability set. This is
    // the pair the driver actually met: a 1.1 controller sending 57 bytes to a reader expecting 60.
    CHECK(throws<scopelink::ProtocolError>(
        [] { scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(3, 1)), capabilitiesFor(3)); }));

    CHECK(throws<scopelink::ProtocolError>(
        [] {
            scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(3, 0)),
                                     capabilitiesFor(3, -1, 1, 1));
        }));

    // And the pair the third motor makes: a generation 4 frame is eight bytes longer than the generation 3
    // frame on the same interface version, so neither capability set will decode the other's.
    CHECK(throws<scopelink::ProtocolError>(
        [] {
            scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(4, 1)),
                                     capabilitiesFor(3, -1, 1, 1));
        }));

    CHECK(throws<scopelink::ProtocolError>(
        [] {
            scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(3, 1)),
                                     capabilitiesFor(4, -1, 1, 1));
        }));
}

void testTemperaturePlausibility()
{
    Frame payload = statusPayload(3);

    // 0xFFFF is what a failed MLX90614 reports, and it converts to a plausible looking -273.0 C.
    payload[22] = 0xff;
    payload[23] = 0xff;

    const scopelink::Status status =
        scopelink::Status::parse(reply(scopelink::command::Status, payload), capabilitiesFor(3));

    CHECK(!status.ambientTemperatureValid);
    CHECK(status.mirrorTemperatureValid);
}

void testProtocolFraming()
{
    const Frame request = scopelink::Protocol::buildRequest(scopelink::command::Motor, Frame{ 0, 4 });

    CHECK(request.size() == 6);
    CHECK(request[0] == 0xaa);
    CHECK(request[1] == 0x55);
    CHECK(request[2] == scopelink::command::Motor);
    CHECK(request[3] == 2);

    MockTransport transport;
    scopelink::Protocol protocol(transport);

    transport.responses.push_back(reply(scopelink::command::Status, Frame{ 1, 2, 3, 4 }));

    const Frame response = protocol.transact(scopelink::command::Status, {}, 8);

    CHECK(response.size() == 8);
    CHECK(response[7] == 4);
    CHECK(transport.writes.size() == 1);
    CHECK(protocol.transactionCount() == 1);
}

void testProtocolResync()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);

    // A stale frame in front of the real one is stepped over rather than losing the transaction.
    Frame noisy      = { 0x00, 0xaa, 0x11 };
    const Frame good = reply(scopelink::command::Status, Frame{ 9 });

    noisy.insert(noisy.end(), good.begin(), good.end());
    transport.responses.push_back(noisy);

    const Frame response = protocol.transact(scopelink::command::Status, {}, 5);

    CHECK(response.size() == 5);
    CHECK(response[4] == 9);
    CHECK(protocol.retryCount() == 0);
}

void testProtocolWrongCommandEcho()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);

    // Three answers for the wrong command, which is the whole retry budget.
    for (int attempt = 0; attempt < 3; attempt++)
        transport.responses.push_back(reply(scopelink::command::Fan, Frame{ 1 }));

    CHECK(throws<scopelink::CommunicationError>([&] { protocol.transact(scopelink::command::Status, {}, 5); }));

    CHECK(protocol.failureCount() == 1);
    CHECK(protocol.consecutiveFailures() == 1);
}

void testProtocolRetryAndRecovery()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);

    // Nothing for the first attempt, which times out, then a good frame.
    transport.responses.push_back(Frame{});
    transport.responses.push_back(reply(scopelink::command::Status, Frame{ 7 }));

    const Frame response = protocol.transact(scopelink::command::Status, {}, 5);

    CHECK(response[4] == 7);
    CHECK(protocol.retryCount() == 1);

    // A timeout does not recycle the port on the first attempt; that is what makes a single lost frame
    // cheap to recover from.
    CHECK(transport.reopenCount == 0);
}

void testProtocolAbandonsUnrecoverablePort()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);

    transport.reopenSucceeds = false;

    CHECK(throws<scopelink::CommunicationError>([&] { protocol.transact(scopelink::command::Status, {}, 5); }));

    // Two timeouts, then the second attempt asks for a port recycle, is told it cannot be done, and
    // stops rather than spending the last attempt on a port that is not coming back.
    CHECK(transport.reopenCount == 1);
    CHECK(protocol.retryCount() == 2);
}

void testProtocolVariableLength()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);

    transport.responses.push_back(reply(scopelink::command::ReadDtc, Frame(40, 0x5a)));

    const Frame response = protocol.transactVariable(scopelink::command::ReadDtc, {}, 6);

    CHECK(response.size() == 44);
}

/** @brief Builds the answer to a configuration identifier read of a given width. */
Frame didReply(uint32_t did, int length, unsigned value)
{
    Frame payload{ 0x00, static_cast<uint8_t>(did >> 8), static_cast<uint8_t>(did & 0xff) };

    if (length == 1)
        payload.push_back(static_cast<uint8_t>(value & 0xff));
    else if (length == 2)
        put16(payload, value);
    else
        put32(payload, value);

    return reply(scopelink::command::DataIdentifier, payload);
}

/**
 * @brief Queues the answers to the motor assignment a controller on interface 1.1 is asked for.
 *
 * The order is the order MotorRoles::of asks in, because the mock transport answers a queue rather than a
 * request. The assignment itself is the firmware's default one - motor 1 focuses, motor 2 is the flap,
 * nothing rotates - written with "not used" spelled as the motor count, which is what the firmware means
 * by it and therefore what the narrowed catalogue writes.
 */
void scriptMotorRoles(MockTransport &transport, int motorCount)
{
    const unsigned unused = static_cast<unsigned>(motorCount);

    transport.responses.push_back(didReply(0x0400, 1, 0));      // the focuser is motor 1
    transport.responses.push_back(didReply(0x0410, 1, unused)); // nothing rotates
    transport.responses.push_back(didReply(0x0401, 2, 1));      // one motor step per focuser step
    transport.responses.push_back(didReply(0x0411, 4, 51200));  // steps per rotator revolution

    transport.responses.push_back(didReply(0x0420, 1, 1)); // the flap's first part is motor 2
    transport.responses.push_back(didReply(0x0423, 1, unused));

    if (motorCount > 2)
        transport.responses.push_back(didReply(0x0426, 1, unused));
}

/** @brief Queues the exact exchange Device::open() performs, for a given hardware generation. */
void scriptOpen(MockTransport &transport, int generation, int interfaceMinor = 0)
{
    transport.responses.push_back(
        reply(scopelink::command::InterfaceVersion,
              Frame{ static_cast<uint8_t>(generation), 0, 1, static_cast<uint8_t>(interfaceMinor) }));
    transport.responses.push_back(
        reply(scopelink::command::HardwareIdentification, Frame{ 0xde, 0xad, 0xbe, 0xef, 1, 2, 3, 4, 5, 6, 7, 8 }));

    Frame software(40, 0);
    const char *text = "SCOPELINK TEST 1.0";

    std::memcpy(software.data(), text, std::strlen(text));
    transport.responses.push_back(reply(scopelink::command::SoftwareIdentification, software));

    // Only generation 3 is asked whether its temperature sensor is fitted.
    if (generation > 2)
        transport.responses.push_back(reply(scopelink::command::DataIdentifier, Frame{ 0x00, 0x03, 0x02, 0x01 }));

    transport.responses.push_back(reply(scopelink::command::SetRtc, Frame{ 1 }));

    // Which motor drives what, read behind the clock check and in front of the first status sample. Only
    // a controller on interface 1.1 holds it; before that the assignment is stated rather than read, and
    // no transaction is sent.
    if (interfaceMinor >= 1)
        scriptMotorRoles(transport, (generation > 3) ? 3 : 2);

    transport.responses.push_back(reply(scopelink::command::Status, statusPayload(generation, interfaceMinor)));
}

void testDeviceOpen()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    scriptOpen(transport, 3);
    device.open();

    CHECK(device.isIdentified());
    CHECK(device.identification().hardwareMajor == 3);
    CHECK(device.identification().hardwareIdentifier == "DEADBEEF0102030405060708");
    CHECK(device.identification().softwareIdentifier == "SCOPELINK TEST 1.0");
    CHECK(device.capabilities().hasTemperatureSensor);
    CHECK(device.capabilities().hasUsbHub);
    CHECK(device.hasFreshStatus());
    CHECK(device.status().motor(0).position == 33000);

    // A controller that will not take the clock is not a ScopeLink, which is where that check earns its
    // keep: it is the first command whose answer is specific rather than merely well formed.
    MockTransport wrong;
    scopelink::Protocol wrongProtocol(wrong);
    scopelink::Device wrongDevice(wrongProtocol);

    scriptOpen(wrong, 3);
    wrong.responses[4] = reply(scopelink::command::SetRtc, Frame{ 0 });

    CHECK(throws<scopelink::ProtocolError>([&] { wrongDevice.open(); }));
}

/**
 * @brief One named field of a freeze frame, or the fallback when the controller does not send it.
 *
 * Written here rather than reached for in Fault, whose own lookup is private: what a test needs is to
 * assert that a name maps to a byte, and a field that is absent has to be distinguishable from one that
 * happens to read zero. Distinct from snapshotField below, which answers the recorded vectors in text.
 */
int snapshotByte(const scopelink::Fault &fault, const char *name, int fallback = 0)
{
    for (const scopelink::FaultSnapshotField &entry : fault.snapshot)
    {
        if (entry.name == name)
            return entry.value;
    }

    return fallback;
}

void testFaultDecoding()
{
    const int layouts[][2] = { { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 1 } };

    for (const auto &layout : layouts)
    {
        const int generation     = layout[0];
        const int interfaceMinor = layout[1];

        const scopelink::Capabilities capabilities = capabilitiesFor(generation, -1, 1, interfaceMinor);

        MockTransport transport;
        scopelink::Protocol protocol(transport);
        scopelink::Device device(protocol);

        scriptOpen(transport, generation, interfaceMinor);
        device.open();

        Frame payload = { 0x07, 0x01 };

        // One record: code 4, no additional data, seen 3 times, currently active.
        put16(payload, 4);
        put16(payload, 0);
        payload.push_back(3);
        payload.push_back(1);

        Frame snapshot(capabilities.dtcSnapshotLength, 0);

        // The clock fields sit four bytes later on generation 3 because of the two auxiliary voltages
        // ahead of them. That drift is exactly what made the fault store unreadable on older hardware
        // before the field list was walked in order rather than indexed with fixed offsets.
        const size_t yearOffset = (generation > 2) ? 20 : 16;

        snapshot[yearOffset]     = 0x07;
        snapshot[yearOffset + 1] = 0xea; // 2026
        snapshot[yearOffset + 2] = 8;
        snapshot[yearOffset + 3] = 11;
        snapshot[yearOffset + 4] = 21;
        snapshot[yearOffset + 5] = 45;
        snapshot[yearOffset + 6] = 30;

        // The bytes behind the clock, which are the motors and the flat box in one order or the other -
        // five of them where there are two motors, seven where there are three. Every one is distinct and
        // none is zero, because zero is what they all hold on a controller whose motors are halted and
        // whose flat box is off - which is every fault anyone has recorded so far, and is why reading them
        // in the wrong order was invisible until it was looked for.
        const size_t motorBlock = yearOffset + 7;
        const size_t motorBytes = (generation > 3) ? 7u : 5u;

        for (size_t byte = 0; byte < motorBytes; byte++)
            snapshot[motorBlock + byte] = static_cast<uint8_t>(0x11 * (byte + 1));

        payload.insert(payload.end(), snapshot.begin(), snapshot.end());

        // readAll() takes its own status sample first, to learn how many faults are stored.
        transport.responses.push_back(reply(scopelink::command::Status, statusPayload(generation, interfaceMinor)));
        transport.responses.push_back(reply(scopelink::command::ReadDtc, payload));

        const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

        CHECK(faults.size() == 1);

        if (faults.empty())
            continue;

        const scopelink::Fault &fault = faults[0];

        CHECK(fault.code == scopelink::FaultCode::Mlx90614CommunicationTimeout);

        // The number that arrived, kept beside the identity it was looked up as. They agree here because 4
        // is the sensor timeout on both numberings, which is what makes this fault safe to record against
        // every generation - see the fault numbering test for the pair that do not agree.
        CHECK(fault.wireCode == 4);
        CHECK(fault.occurrenceCount == 3);
        CHECK(fault.isActive);
        CHECK(fault.timestamp() == "2026-08-11 21:45:30");
        CHECK(fault.codeName().find("Infrared") != std::string::npos);

        // The motors were brought together and the flat box moved after them when the smart switch
        // monitoring went in, so generation 3 reads the same five bytes in a different order. Checked by
        // name rather than by offset, because a wrong order here is not a wrong length: it names three
        // fields after the wrong bytes and decodes without complaint.
        CHECK(snapshotByte(fault, "Motor 1 status") == 0x11);
        CHECK(snapshotByte(fault, "Motor 1 load") == 0x22);

        if (generation > 3)
        {
            // Three motors together, and only then the flat box. Nothing but the field list places these:
            // this freeze frame is 48 bytes, and so is a generation 3 controller's on interface 1.0.
            CHECK(snapshotByte(fault, "Motor 2 status") == 0x33);
            CHECK(snapshotByte(fault, "Motor 2 load") == 0x44);
            CHECK(snapshotByte(fault, "Motor 3 status") == 0x55);
            CHECK(snapshotByte(fault, "Motor 3 load") == 0x66);
            CHECK(snapshotByte(fault, "Flatbox duty") == 0x77);
            CHECK(snapshotByte(fault, "USB DS6 power active", -1) == 0);
        }
        else if (generation > 2)
        {
            CHECK(snapshotByte(fault, "Motor 2 status") == 0x33);
            CHECK(snapshotByte(fault, "Motor 2 load") == 0x44);
            CHECK(snapshotByte(fault, "Flatbox duty") == 0x55);
            CHECK(snapshotByte(fault, "Motor 3 status", -1) == -1);
            CHECK(snapshotByte(fault, "USB DS6 power active", -1) == -1);
        }
        else
        {
            CHECK(snapshotByte(fault, "Flatbox duty") == 0x33);
            CHECK(snapshotByte(fault, "Motor 2 status") == 0x44);
            CHECK(snapshotByte(fault, "Motor 2 load") == 0x55);
        }

        // A field the controller does not send is absent rather than zero, so asking for it returns the
        // fallback. That is what says the digital inputs are gone on 1.1 and not merely reading as zero.
        CHECK(snapshotByte(fault, "Digital input 1 state", -1) == ((interfaceMinor >= 1) ? -1 : 0));

        // The field count separates generation 2 from the rest and the two interface versions of
        // generation 3 from each other - and it does not separate generation 4 from a generation 3
        // controller on 1.0, which both hold 36 fields in 48 bytes. That pair is what the named checks
        // above are for: a count would have passed either way round.
        size_t expectedFields = 24;

        if (generation > 3)
            expectedFields = 36; // no digital inputs, but a third motor and four more ports
        else if (generation > 2)
            expectedFields = (interfaceMinor >= 1) ? 32u : 36u;

        CHECK(fault.snapshot.size() == expectedFields);
    }
}

/**
 * @brief Every simulated controller, opened and read the way the driver opens and reads a real one.
 *
 * The simulator is the only thing that speaks generation 4 or interface 1.1: no unit of either has been
 * recorded, and until one is, a frame layout nothing sends is a layout nothing checks. So it is held to
 * the parsers here rather than only under a person watching the pseudo terminal tool - the parsers are
 * the thing it exists to feed, and a simulator that has drifted from them proves nothing about either.
 *
 * Deliberately not a check that the simulator sends particular bytes. What is asserted is that the driver
 * can open it, that the frames come out the length its own capability set says, and that its fault store
 * decodes - which is the whole of what a developer with no hardware is relying on it for.
 */
void testSimulatedControllers()
{
    const int layouts[][2] = { { 2, 0 }, { 3, 0 }, { 3, 1 }, { 4, 1 } };

    for (const auto &layout : layouts)
    {
        const int generation     = layout[0];
        const int interfaceMinor = layout[1];

        scopelink::SimulatedTransport transport(generation, interfaceMinor);
        scopelink::Protocol protocol(transport);
        scopelink::Device device(protocol);

        device.open();

        const scopelink::Identification &identification = device.identification();
        const scopelink::Capabilities &capabilities     = device.capabilities();

        CHECK(identification.hardwareMajor == generation);
        CHECK(identification.interfaceMinor == interfaceMinor);

        // Derived by the driver from what the simulator answered, rather than handed to either of them.
        CHECK(capabilities.motorCount == ((generation > 3) ? 3 : 2));
        CHECK(capabilities.usbDownstreamPortCount == ((generation > 3) ? 6 : ((generation > 2) ? 2 : 0)));
        CHECK(capabilities.hasConfigurableMotorRoles == (interfaceMinor >= 1));

        // open() takes its first sample before it returns, so reaching here already means the status frame
        // was the length this capability set expects. The motor count is the visible half of that.
        const scopelink::Status &status = device.status();

        CHECK(status.motorCount() == capabilities.motorCount);
        CHECK(status.usbDownstreamPortCount() == capabilities.usbDownstreamPortCount);

        // Every simulated controller powers all its downstream ports, which is what makes the sixth one
        // worth asking about: a reader that stopped at two would answer false for it.
        for (int port = 0; port < status.usbDownstreamPortCount(); port++)
            CHECK(status.usbPowerActive(port));

        // The flap state, which only a controller holding the assignment sends. The simulated flap starts
        // shut at zero against a calibrated travel, and says so - a state the motor reading beside it
        // cannot be asked for, which is the whole reason the byte exists.
        CHECK(status.flapState
              == ((interfaceMinor >= 1) ? scopelink::FlapState::Closed : scopelink::FlapState::Unknown));

        // A move on the last motor this controller has, which on generation 4 is the one that only exists
        // there. Answered rather than refused is the whole assertion: the simulator addresses its motors
        // by index, so a controller with three has to take a command for the third.
        const int last = capabilities.motorCount - 1;

        device.moveMotor(last, 4321);
        device.syncMotor(last, 4321);

        CHECK(device.refreshStatus().motor(last).position == 4321);

        // And the fault store, which is where the freeze frame layout and the fault numbering both land.
        const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

        CHECK(faults.size() == 1);

        if (faults.empty())
            continue;

        // The sensor timeout, which the simulator holds as an identity and serves as this controller's own
        // number for it. They agree on every generation so far, and the translation is what will keep them
        // agreeing when they stop being the same number.
        CHECK(faults[0].code == scopelink::FaultCode::Mlx90614CommunicationTimeout);
        CHECK(faults[0].wireCode == scopelink::Fault::wireCodeOf(faults[0].code, generation));
        CHECK(faults[0].isActive);

        // The freeze frame decoder checks its own total against the capability set, so a snapshot that
        // came back at all is one whose field list consumed exactly the bytes the simulator sent.
        CHECK(!faults[0].snapshot.empty());
        CHECK(snapshotByte(faults[0], "Motor 3 status", -1) == ((generation > 3) ? 0 : -1));
        CHECK(snapshotByte(faults[0], "USB DS6 power active", -1) == ((generation > 3) ? 0 : -1));
        CHECK(snapshotByte(faults[0], "Digital input 1 state", -1) == ((interfaceMinor >= 1) ? -1 : 0));
    }
}

/**
 * @brief The two fault numberings, and the six place shift between them.
 *
 * The reason FaultCode stopped being a cast of the wire value. Nothing about a frame length or a field
 * count catches this one: a generation 4 controller reporting an over temperature error sends 32, and the
 * older table reads 32 as a number it has no name for at all - while 26, which that controller means as an
 * auxiliary output open load, reads as the over temperature error. Both are plausible faults to see, and
 * neither is the one that happened.
 */
void testFaultNumbering()
{
    // Every controller before generation 4 numbers the faults in the order they were introduced, so on
    // those the wire number and the identity happen to be the same number.
    CHECK(scopelink::Fault::codeFor(4, 3) == scopelink::FaultCode::Mlx90614CommunicationTimeout);
    CHECK(scopelink::Fault::codeFor(26, 3) == scopelink::FaultCode::EcuOvertemperatureError);
    CHECK(scopelink::Fault::wireCodeOf(scopelink::FaultCode::EcuOvertemperatureError, 3) == 26);

    // Generation 4 inserted the third motor's two failures and four more USB ports where they belonged
    // rather than appending them, which moved everything below by six.
    CHECK(scopelink::Fault::codeFor(9, 4) == scopelink::FaultCode::MotorDriver3CommunicationFailure);
    CHECK(scopelink::Fault::codeFor(18, 4) == scopelink::FaultCode::UsbHubDs6Overcurrent);
    CHECK(scopelink::Fault::codeFor(26, 4) == scopelink::FaultCode::SmartSwitchAuxAOpenLoad);
    CHECK(scopelink::Fault::wireCodeOf(scopelink::FaultCode::EcuOvertemperatureError, 4) == 32);

    // The first nine agree, which is why a store holding one early fault looks right whichever table read
    // it - and is why this went unnoticed for as long as the only recorded faults were early ones.
    for (int code = 0; code < 9; code++)
        CHECK(scopelink::Fault::codeFor(code, 3) == scopelink::Fault::codeFor(code, 4));

    // Faults one controller has and the other does not, in both directions.
    CHECK(scopelink::Fault::wireCodeOf(scopelink::FaultCode::MotorDriver3CommunicationFailure, 3) == -1);
    CHECK(scopelink::Fault::wireCodeOf(scopelink::FaultCode::MotorConfigurationInvalid, 3) == -1);
    CHECK(scopelink::Fault::wireCodeOf(scopelink::FaultCode::MotorConfigurationInvalid, 4) == 34);

    // A number past the end of a table is named as unknown rather than cast into a fault that does not
    // exist. A controller running firmware newer than this driver is the ordinary way to meet one.
    CHECK(scopelink::Fault::codeFor(28, 3) == scopelink::FaultCode::Unknown);
    CHECK(scopelink::Fault::codeFor(35, 4) == scopelink::FaultCode::Unknown);
    CHECK(scopelink::Fault::codeFor(-1, 3) == scopelink::FaultCode::Unknown);
    CHECK(std::string(scopelink::Fault::nameOf(scopelink::FaultCode::Unknown)) == "Unknown fault");
}

/** @brief The generated descriptor of one identifier, so a test can build a Did without a controller. */
const scopelink::DidDescriptor &descriptorFor(uint32_t id)
{
    const scopelink::DidDescriptor *descriptor = scopelink::findDidDescriptor(id);

    if (descriptor == nullptr)
    {
        // Nothing sensible can be returned for an identifier the firmware description does not hold, and
        // every caller here names one that has to be in it.
        printf("  no generated descriptor for 0x%04X\n", id);
        throw std::runtime_error("missing descriptor");
    }

    return *descriptor;
}

/**
 * @brief Checks the generated table against itself.
 *
 * The generator refuses to write a table that fails any of this, so these run here only to catch a
 * committed file that was edited by hand or produced by an older script - which is the one failure the
 * generator cannot catch, because upstream never runs it.
 */
/**
 * @brief The rules a motor assignment is held to, which are the firmware's MFNC_CheckSelections.
 *
 * Checked against the rules rather than against a controller, because they are what both drivers and the
 * firmware have to agree on: a driver that accepted an assignment the controller refuses would offer a
 * device that answers every command with "not configured".
 */
void testMotorRoleRules()
{
    using scopelink::MotorRoles;

    // Three motors, so "not used" is 3. The default assignment: motor 1 focuses, motor 2 opens the flap,
    // nothing rotates and the second and third flap parts are unused.
    CHECK(MotorRoles::problemWith(3, { 0, 3, 1, 3, 3 }).empty());

    // Every motor claimed, by three different functions.
    CHECK(MotorRoles::problemWith(3, { 0, 1, 2, 3, 3 }).empty());

    // A flap of three parts, which is the one assignment that leaves nothing to focus with.
    CHECK(MotorRoles::problemWith(3, { 3, 3, 0, 1, 2 }).empty());

    // On two motors the highest index is 1 and 2 is what "not used" is spelled with, so the same default
    // assignment is a different set of numbers.
    CHECK(MotorRoles::problemWith(2, { 0, 2, 1, 2 }).empty());

    // 1. A motor this controller does not have. Three is a motor on a three motor board and nothing at all
    //    on a two motor one, which is the trap the narrowed catalogue exists to close.
    CHECK(!MotorRoles::problemWith(2, { 3, 2, 1, 2 }).empty());
    CHECK(!MotorRoles::problemWith(3, { 4, 3, 1, 3, 3 }).empty());
    CHECK(!MotorRoles::problemWith(3, { -1, 3, 1, 3, 3 }).empty());

    // 2. A hole in the flap sequence: part 2 unused while part 3 is not. The delays would then describe a
    //    sequence with a gap in the middle of it, which is not a thing that can be run.
    CHECK(!MotorRoles::problemWith(3, { 3, 3, 0, 3, 1 }).empty());

    // 3. One motor claimed twice, which is the whole reason this block of parameters exists.
    CHECK(!MotorRoles::problemWith(3, { 0, 0, 1, 3, 3 }).empty());
    CHECK(!MotorRoles::problemWith(3, { 0, 3, 1, 1, 3 }).empty());

    // The complaint names what is wrong in the controller's own terms, because it is shown to a user who
    // has to go and correct it.
    CHECK(MotorRoles::problemWith(3, { 0, 0, 1, 3, 3 }).find("claimed by more than one") != std::string::npos);
}

/** @brief What an assignment reports once it has been read, and what a bad one reports instead. */
void testMotorRoles()
{
    using scopelink::MotorFunction;
    using scopelink::MotorRoles;

    const scopelink::Capabilities two   = capabilitiesFor(3, -1, 1, 1);
    const scopelink::Capabilities three = capabilitiesFor(4, -1, 1, 1);

    CHECK(two.motorCount == 2);
    CHECK(three.motorCount == 3);

    // Only as many flap parts as there are motors, because a part needs one to drive it - and a controller
    // that cannot have a third part does not carry its identifiers at all.
    CHECK(MotorRoles::flapPartsOn(two) == 2);
    CHECK(MotorRoles::flapPartsOn(three) == 3);
    CHECK(MotorRoles::selectionDids(two).size() == 4);
    CHECK(MotorRoles::selectionDids(three)[MotorRoles::FirstFlapSelection + 2] == 0x0426);

    // The assignment every controller before interface 1.1 was built with, stated rather than read.
    const MotorRoles legacy = MotorRoles::legacy(two);

    CHECK(!legacy.isConfigurable());
    CHECK(legacy.isValid());
    CHECK(legacy.hasFocuser() && (legacy.focuserMotor().value() == 0));
    CHECK(!legacy.hasRotator());
    CHECK(legacy.hasFlap() && (legacy.flapMotors().size() == 1) && (legacy.flapMotors()[0] == 1));
    CHECK(legacy.focuserStepMultiplier() == 1);

    // Generation 2 is the oldest controller this driver supports and it already has the second motor, so
    // every controller the fixed assignment describes has a flap on it.
    CHECK(MotorRoles::legacy(capabilitiesFor(2)).hasFlap());

    // Everything assigned, on three motors: the focuser, the rotator, and a flap of one part.
    const MotorRoles full = MotorRoles::build(three, 0, 2, { 1, 3, 3 }, 8, 51200);

    CHECK(full.isConfigurable());
    CHECK(full.isValid());
    CHECK(full.focuserMotor().value() == 0);
    CHECK(full.rotatorMotor().value() == 2);
    CHECK(full.flapMotors().size() == 1);
    CHECK(full.focuserStepMultiplier() == 8);
    CHECK(full.rotatorStepsPerRevolution() == 51200);

    // The reverse view, which is what the calibration page and the fault snapshots need: they have a motor
    // and want a name for it.
    CHECK(full.functionOf(0) == MotorFunction::Focuser);
    CHECK(full.functionOf(1) == MotorFunction::Flap);
    CHECK(full.functionOf(2) == MotorFunction::Rotator);

    // A flap of two parts, in opening order, and nothing left to focus with.
    const MotorRoles split = MotorRoles::build(three, 3, 3, { 0, 1, 3 }, 1, 51200);

    CHECK(split.isValid());
    CHECK(!split.hasFocuser());
    CHECK(!split.hasRotator());
    CHECK(split.flapMotors().size() == 2);
    CHECK(split.flapMotors()[0] == 0);
    CHECK(split.flapMotors()[1] == 1);

    // An assignment that breaks a rule offers nothing at all, rather than offering the part of it that
    // happens to make sense. The controller is already refusing to drive any of it.
    const MotorRoles broken = MotorRoles::build(three, 0, 0, { 1, 3, 3 }, 1, 51200);

    CHECK(broken.isConfigurable());
    CHECK(!broken.isValid());
    CHECK(!broken.problem().empty());
    CHECK(!broken.hasFocuser());
    CHECK(!broken.hasRotator());
    CHECK(!broken.hasFlap());

    // A controller that will not answer one of the identifiers is a failed connection rather than a
    // guessed assignment: guessing would mean handing a client a focuser that might be the flap.
    CHECK(throws<scopelink::CommunicationError>(
        [&]
        {
            MotorRoles::of(three, [](uint32_t did, int) -> std::optional<uint32_t>
                           { return (did == 0x0411) ? std::optional<uint32_t>() : std::optional<uint32_t>(0); });
        }));

    // And a controller from before the assignment existed is answered without a transaction being sent.
    bool asked = false;

    const MotorRoles older = MotorRoles::of(capabilitiesFor(3, -1, 1, 0),
                                            [&asked](uint32_t, int) -> std::optional<uint32_t>
                                            {
                                                asked = true;
                                                return 0;
                                            });

    CHECK(!asked);
    CHECK(!older.isConfigurable());
}

/**
 * @brief The function commands: what goes on the wire, and what a refusal is turned into.
 *
 * The payload is checked byte for byte because it is the half of the protocol nothing else would catch -
 * a controller answers a wrong sub-function with a refusal that looks exactly like a legitimate one.
 */
void testFunctionCommands()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    scriptOpen(transport, 4, 1);
    device.open();

    const size_t opened = transport.writes.size();

    // Accepted: the response payload is a single FunctionResponse::Ok.
    const Frame accepted = reply(scopelink::command::Function, Frame{ 5 });

    transport.responses.push_back(accepted);
    device.moveFocuser(12345);

    CHECK(transport.writes.size() == (opened + 1));

    // Header, then the function, the sub-function and a big endian position.
    const Frame &move = transport.writes[opened];

    CHECK(move.size() == 10);
    CHECK(move[2] == scopelink::command::Function);
    CHECK(move[3] == 6);
    CHECK(move[4] == scopelink::function::Focuser);
    CHECK(move[5] == scopelink::function::Move);
    CHECK(scopelink::byte_order::toInt(move, 6, 4) == 12345);

    // The flap carries no position at all, which is the point of it: how far each part goes and in what
    // order is the controller's business.
    transport.responses.push_back(accepted);
    device.openFlap();

    const Frame &open = transport.writes[opened + 1];

    CHECK(open.size() == 6);
    CHECK(open[3] == 2);
    CHECK(open[4] == scopelink::function::Flap);
    CHECK(open[5] == scopelink::function::Open);

    transport.responses.push_back(accepted);
    device.closeFlap();
    CHECK(transport.writes[opened + 2][5] == scopelink::function::Close);

    transport.responses.push_back(accepted);
    device.haltRotator();
    CHECK(transport.writes[opened + 3][4] == scopelink::function::Rotator);
    CHECK(transport.writes[opened + 3][5] == scopelink::function::Halt);

    transport.responses.push_back(accepted);
    device.syncFocuser(-7);
    CHECK(scopelink::byte_order::toInt(transport.writes[opened + 4], 6, 4) == -7);

    // Every refusal is a condition a user can act on, so every one of them is decoded and named. This is
    // what the motor command could not do: it answers with a byte that nothing reads, so a move the
    // controller would not make was reported as a move that had started.
    const struct
    {
            uint8_t code;
            const char *expected;
    } refusals[] = {
        { 0, "no motor is assigned" }, { 1, "not been calibrated" }, { 2, "outside" },
        { 3, "already moving" },       { 4, "refused the command" }, { 9, "unknown code 9" },
    };

    for (const auto &refusal : refusals)
    {
        transport.responses.push_back(reply(scopelink::command::Function, Frame{ refusal.code }));

        std::string reported;

        try
        {
            device.moveFocuser(10);
        }
        catch (const scopelink::CommunicationError &error)
        {
            reported = error.what();
        }

        CHECK(reported.find(refusal.expected) != std::string::npos);
        CHECK(reported.find("focuser") != std::string::npos);
    }
}

/**
 * @brief The simulated controller resolves a function to whichever motor its assignment names, and picks
 *        up a new assignment when it is restarted rather than when it is written.
 *
 * Two assertions matter, and the second one was measured on hardware before it was written down here. A
 * driver that baked the mapping in would pass the first half: a reassigned flap has to be opened by the
 * motor the assignment now names and not by the one an old mapping did. A driver that believed a write
 * was enough would pass the second: the controller reads its configuration when it starts and drives
 * from that copy, so until it is restarted a new assignment reads back and changes nothing at all.
 */
void testSimulatedFunctions()
{
    scopelink::SimulatedTransport transport(4, 1);
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    device.open();

    /** @brief Waits for the flap to settle, or gives up. The simulated motors advance in real time. */
    const auto settle = [&device](scopelink::FlapState wanted)
    {
        for (int poll = 0; poll < 400; poll++)
        {
            if (device.refreshStatus().flapState == wanted)
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return false;
    };

    /** @brief Writes one identifier, for the setting up this test does before it drives anything. */
    const auto write = [&device](uint32_t did, int value)
    {
        scopelink::Did identifier(*scopelink::findDidDescriptor(did));

        identifier.setValue(value);

        return identifier.write(device);
    };

    const scopelink::MotorRoles &roles = device.roles();

    CHECK(roles.isConfigurable());
    CHECK(roles.isValid());
    CHECK(roles.focuserMotor().value() == 0);
    CHECK(roles.flapMotors().size() == 1);
    CHECK(roles.flapMotors()[0] == 1);
    CHECK(!roles.hasRotator());

    // The travels are made short and the motors are put at zero before anything is driven. A simulated
    // motor advances at a fixed rate in real time, and this test is about which motor moves rather than
    // about how long a real flap takes to open.
    CHECK(write(scopelink::motor::maximumPositionDid(1), 60));
    CHECK(write(scopelink::motor::maximumPositionDid(2), 60));

    device.syncMotor(1, 0);
    device.syncMotor(2, 0);

    // The focuser moves the motor the assignment names, and nothing else moves. Both commands are
    // addressed to the focuser rather than to a motor, so the resolution is the simulator's.
    device.syncFocuser(100);
    CHECK(device.refreshStatus().motor(0).position == 100);

    const int elsewhere = device.status().motor(2).position;

    device.moveFocuser(300);

    for (int poll = 0; (poll < 400) && (device.refreshStatus().motor(0).position != 300); poll++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    CHECK(device.status().motor(0).position == 300);
    CHECK(device.status().motor(2).position == elsewhere);

    // The flap opens against its own calibrated travel, which the driver never has to name.
    device.openFlap();

    CHECK(settle(scopelink::FlapState::Open));
    CHECK(device.status().motor(1).position == 60);

    device.closeFlap();

    CHECK(settle(scopelink::FlapState::Closed));
    CHECK(device.status().motor(1).position == 0);

    // Nothing is assigned to the rotator, and a request for it is refused rather than applied to whichever
    // motor a fixed mapping would have named.
    CHECK(throws<scopelink::CommunicationError>([&] { device.moveRotator(30); }));

    // Now assign the rotator to the motor nothing is using. The controller reads the new assignment back
    // at once - it is stored, and storing it is the whole of what a write does - and goes on driving the
    // one it started with, so the rotator it now says it has is still refused. Measured on a generation 3
    // unit on interface 1.1: a rotator assigned while the controller ran was refused as "no motor is
    // assigned to it" until the controller had been restarted, after which the same command worked.
    CHECK(write(scopelink::MotorRoles::RotatorMotorDid, 2));
    CHECK(device.refreshMotorRoles().hasRotator());
    CHECK(throws<scopelink::CommunicationError>([&] { device.moveRotator(30); }));

    // The restart is what makes it read its configuration again, and the same command then works.
    CHECK(device.requestReset());

    device.moveRotator(30);

    for (int poll = 0; (poll < 400) && (device.refreshStatus().motor(2).position != 30); poll++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    CHECK(device.status().motor(2).position == 30);

    // The flap goes the same way round. Moving it onto the third motor means giving that motor up as the
    // rotator first, which is two writes rather than one - the same sequencing the driver's own parameter
    // page has to do, because no motor may be claimed by two functions at any point along the way.
    CHECK(write(scopelink::MotorRoles::RotatorMotorDid, 3));
    CHECK(write(0x0420, 2));

    CHECK(device.refreshMotorRoles().flapMotors()[0] == 2);
    CHECK(!device.roles().hasRotator());

    // Still the old assignment until it is restarted, so this opens motor 2 and leaves motor 3 where the
    // rotator move left it.
    device.openFlap();

    CHECK(settle(scopelink::FlapState::Open));
    CHECK(device.status().motor(1).position == 60);
    CHECK(device.status().motor(2).position == 30);

    const int abandoned = device.status().motor(1).position;

    CHECK(device.requestReset());

    device.openFlap();

    CHECK(settle(scopelink::FlapState::Open));
    CHECK(device.status().motor(2).position == 60);

    // Left open, because nothing closes a motor that has stopped being part of the flap. It is worth
    // asserting: it is the state a user who reassigns a flap without moving the mechanism first is left
    // with, and the reason the driver tells them to recalibrate what they have moved.
    CHECK(device.status().motor(1).position == abandoned);
}

/**
 * @brief A motor selection offers the motors the controller has, and spells "not used" its way.
 *
 * The generated description is a three motor firmware's. Shown to a two motor controller as it stands it
 * would offer a Motor 3 there is no stepper for, and write 3 for "not used" where that firmware means 2.
 */
void testNarrowedMotorSelections()
{
    const scopelink::Capabilities two   = capabilitiesFor(3, -1, 1, 1);
    const scopelink::Capabilities three = capabilitiesFor(4, -1, 1, 1);

    const std::vector<scopelink::Did> onTwo   = scopelink::buildDidCatalogue(two);
    const std::vector<scopelink::Did> onThree = scopelink::buildDidCatalogue(three);

    const auto find = [](const std::vector<scopelink::Did> &catalogue, uint32_t id) -> const scopelink::Did *
    {
        for (const scopelink::Did &identifier : catalogue)
        {
            if (identifier.id() == id)
                return &identifier;
        }

        return nullptr;
    };

    const scopelink::Did *narrowed = find(onTwo, 0x0410);
    const scopelink::Did *whole    = find(onThree, 0x0410);

    CHECK(narrowed != nullptr);
    CHECK(whole != nullptr);

    if ((narrowed == nullptr) || (whole == nullptr))
        return;

    // Two motors and a "not used", against three motors and a "not used".
    CHECK(narrowed->descriptor().optionCount == 3);
    CHECK(whole->descriptor().optionCount == 4);

    // "Not used" keeps its name and moves down to the value this controller spells it with. It is what the
    // firmware defines MFNC_MOTOR_NONE as, which is the motor count.
    CHECK(std::string(narrowed->descriptor().options[2].name) == "MOTOR_SEL_NONE");
    CHECK(narrowed->descriptor().options[2].value == 2);
    CHECK(whole->descriptor().options[3].value == 3);

    // The range follows, so a write of 3 on a two motor controller is refused here rather than by the
    // controller - which would have refused it without telling anybody why.
    CHECK(narrowed->maximum() == 2);
    CHECK(!narrowed->isInRange(3));
    CHECK(whole->isInRange(3));

    // And the default follows too. The rotator's default is "not used", and on this controller that is 2.
    CHECK(narrowed->value() == 2);
    CHECK(whole->value() == 3);

    // Everything that is not a motor selection passes through untouched, options and all.
    const scopelink::Did *direction = find(onTwo, 0x000c);

    CHECK(direction != nullptr);

    if (direction != nullptr)
    {
        CHECK(direction->descriptor().optionCount == 2);
        CHECK(direction->maximum() == 1);
    }
}

void testGeneratedCatalogue()
{
    CHECK(scopelink::catalogue::ParameterCount == 80);
    CHECK(scopelink::catalogue::GroupCount == 13);
    CHECK(std::string(scopelink::catalogue::Product) == "ScopeLink");
    CHECK(scopelink::catalogue::InterfaceMajor == 1);

    // The minor too. Checking only the major is how a table a whole interface version behind sat here
    // unnoticed: 1.0 and 1.1 both read as 1, and nothing else in this tree compares the two.
    CHECK(scopelink::catalogue::InterfaceMinor == 1);

    bool identifiersAreUnique   = true;
    bool rangesHoldTheirDefault = true;
    bool groupsAreDeclared      = true;
    bool scalesDivide           = true;
    bool settingsAreInRange     = true;
    bool switchesAreNamed       = true;

    for (size_t index = 0; index < scopelink::catalogue::ParameterCount; index++)
    {
        const scopelink::DidDescriptor &descriptor = scopelink::catalogue::Parameters[index];

        // findDidDescriptor returns the first match, so this is also the check that there is only one.
        identifiersAreUnique = identifiersAreUnique && (scopelink::findDidDescriptor(descriptor.id) == &descriptor);

        rangesHoldTheirDefault = rangesHoldTheirDefault && (descriptor.minimum <= descriptor.fallback)
                                 && (descriptor.fallback <= descriptor.maximum);

        groupsAreDeclared = groupsAreDeclared && (scopelink::findDidGroup(descriptor.group) != nullptr);
        scalesDivide      = scalesDivide && (descriptor.scale >= 1);

        for (size_t option = 0; option < descriptor.optionCount; option++)
        {
            const int value = descriptor.options[option].value;

            settingsAreInRange = settingsAreInRange && (value >= descriptor.minimum) && (value <= descriptor.maximum);
        }

        if (descriptor.isEnumerated())
            switchesAreNamed = switchesAreNamed && (descriptor.switchProperty != nullptr);
    }

    CHECK(identifiersAreUnique);
    CHECK(rangesHoldTheirDefault);
    CHECK(groupsAreDeclared);
    CHECK(scalesDivide);
    CHECK(settingsAreInRange);
    CHECK(switchesAreNamed);

    // The limits are the firmware's rather than the storage type's, which is the whole point of the
    // table: a byte the controller only takes 0 to 100 in is not offered as 0 to 255.
    const scopelink::DidDescriptor &hold = descriptorFor(0x0001);

    CHECK(hold.minimum == 0);
    CHECK(hold.maximum == 100);
    CHECK(scopelink::Did::maximumOf(hold.type) == 255);

    // A display scale never reaches the wire. It decides how a raw value is shown and how a shown value
    // is turned back, and the two directions have to agree.
    const scopelink::DidDescriptor &velocity = descriptorFor(0x0007);

    CHECK(velocity.scale == 1000);
    CHECK(velocity.decimals() == 3);
    CHECK(velocity.numberFormat() == "%.3f");
    CHECK(velocity.formatValue(1500) == "1.500 rev/s");
    CHECK(velocity.fromDisplay(1.5) == 1500);
    CHECK(velocity.fromDisplay(velocity.toDisplay(2000)) == 2000);

    // A parameter that steps in 0.02 K has no way to hold 2.035, and the nearer step is what was meant.
    // A value that does land on a step survives the trip in both directions unchanged, which is the part
    // that matters: a client that writes back what it was shown does not move the parameter.
    const scopelink::DidDescriptor &delta = descriptorFor(0x0202);

    CHECK(delta.scale == 50);
    CHECK(delta.decimals() == 2);
    CHECK(delta.fromDisplay(2.02) == 101);
    CHECK(delta.formatValue(101) == "2.02 K");
    CHECK(delta.fromDisplay(2.035) == 102);
    CHECK(delta.formatValue(102) == "2.04 K");

    // Far outside is saturated rather than wrapped, so that the range check refuses it instead of a cast
    // turning it into something inside the range.
    CHECK(delta.fromDisplay(1e18) == 2147483647);
    CHECK(delta.fromDisplay(-1e18) == -2147483647 - 1);

    // An enumerated parameter shows the setting's name, and a value no setting stands for is shown as
    // itself rather than as the nearest one.
    const scopelink::DidDescriptor &direction = descriptorFor(0x000c);

    CHECK(direction.isEnumerated());
    CHECK(direction.optionCount == 2);
    CHECK(std::string(direction.switchProperty) == "PARAM_MOTOR1_INVERT_DIRECTION");
    CHECK(std::string(direction.options[1].name) == "MOTOR_DIR_INVERTED");
    CHECK(direction.formatValue(1) == "Inverted");
    CHECK(direction.formatValue(7).find("not a known setting") != std::string::npos);

    // The two motors have identically labelled parameters, so anything shown away from its group heading
    // has to carry that heading with it.
    CHECK(descriptorFor(0x0007).qualifiedLabel() == "Motor 1 - Maximum velocity");
    CHECK(descriptorFor(0x0107).qualifiedLabel() == "Motor 2 - Maximum velocity");

    // The learnt positions sort last, which is also the order a saved configuration is written back in: a
    // motor's maximum position reaches the controller before the last position that has to fit inside it.
    CHECK(std::string(descriptorFor(0x000e).group) == "learnt");
    CHECK(std::string(descriptorFor(0x000f).group) == "motor1");
    CHECK(scopelink::findDidGroup("learnt")->order > scopelink::findDidGroup("motor1")->order);
}

void testDidCatalogue()
{
    const std::vector<scopelink::Did> second = scopelink::buildDidCatalogue(capabilitiesFor(2));
    const std::vector<scopelink::Did> third  = scopelink::buildDidCatalogue(capabilitiesFor(3));

    CHECK(second.size() == 45);
    CHECK(third.size() == 51);

    const auto holds = [](const std::vector<scopelink::Did> &catalogue, uint32_t id)
    {
        for (const scopelink::Did &identifier : catalogue)
        {
            if (identifier.id() == id)
                return true;
        }

        return false;
    };

    CHECK(holds(second, 0x010f));

    // A generation 2 controller answers none of these, so offering them would show four values that can
    // never be read and accept writes that go nowhere.
    CHECK(!holds(second, 0x0600));
    CHECK(!holds(second, 0x0700));
    CHECK(!holds(second, 0x0302));
    CHECK(holds(third, 0x0600));
    CHECK(holds(third, 0x0700));
    CHECK(holds(third, 0x0302));

    // The same board with interface 1.1 firmware on it. Which motor drives the focuser, the rotator and
    // each part of the front flap becomes a setting, so ten more identifiers are answered: the whole
    // 0x0400 block bar the flap's third part, which needs a motor a two motor board does not have.
    const std::vector<scopelink::Did> roles = scopelink::buildDidCatalogue(capabilitiesFor(3, -1, 1, 1));

    CHECK(roles.size() == 61);
    CHECK(holds(roles, 0x0400));
    CHECK(holds(roles, 0x0425));
    CHECK(!holds(roles, 0x0426));
    CHECK(!holds(roles, 0x0428));

    // The same board a firmware version earlier answers none of them, which is the whole point of asking
    // the interface version rather than the generation.
    CHECK(!holds(third, 0x0400));

    // The third motor's block goes with a board that has three, which is generation 4. Interface 1.1 does
    // not bring it, and no controller this driver supports answers it - including the learnt position that
    // sits in a different group but is gated with its motor.
    CHECK(!holds(third, 0x0800));
    CHECK(!holds(roles, 0x0800));
    CHECK(!holds(roles, 0x080e));

    // The catalogue arrives grouped, in the firmware's own group order, which is what lets the driver
    // publish its properties straight out of it without sorting anything itself.
    bool grouped = true;
    int previous = -1;

    for (const scopelink::Did &identifier : third)
    {
        const scopelink::DidGroupInfo *info = scopelink::findDidGroup(identifier.group());

        grouped  = grouped && (info != nullptr) && (info->order >= previous);
        previous = (info == nullptr) ? previous : info->order;
    }

    CHECK(grouped);

    // Every identifier starts at what the firmware would have it be, so a value that has not been read
    // back yet is a plausible one rather than zero.
    CHECK(third.front().value() == third.front().descriptor().fallback);
}

void testDidRanges()
{
    CHECK(scopelink::Did::minimumOf(scopelink::DidType::UInt8) == 0);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::UInt8) == 255);
    CHECK(scopelink::Did::minimumOf(scopelink::DidType::SInt8) == -128);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::SInt8) == 127);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::UInt16) == 65535);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::SInt16) == 32767);

    scopelink::Did identifier(descriptorFor(0x000f));

    CHECK(identifier.elementName() == "DID_000F");
    CHECK(identifier.length() == 4);
    CHECK(identifier.isInRange(40000));
    CHECK(!identifier.isInRange(-1));

    // Two ranges, and the difference between them is what the two checks are for: the firmware's, which
    // decides what may be written, and the storage type's, which decides what an imported file is still
    // recognisably this product's for.
    scopelink::Did current(descriptorFor(0x0001));

    CHECK(current.minimum() == 0);
    CHECK(current.maximum() == 100);
    CHECK(!current.isInRange(200));
    CHECK(current.fitsStorage(200));
    CHECK(!current.fitsStorage(300));
}

void testDidTransactions()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    scopelink::Did identifier(descriptorFor(0x0202));

    transport.responses.push_back(reply(scopelink::command::DataIdentifier, Frame{ 0x00, 0x02, 0x02, 0x00, 0x19 }));

    CHECK(identifier.read(device));
    CHECK(identifier.value() == 25);
    CHECK(identifier.isAvailable());

    CHECK(transport.writes.size() == 1);
    CHECK(transport.writes[0].size() == 7);
    CHECK(transport.writes[0][4] == 0x00); // read operation
    CHECK(transport.writes[0][5] == 0x02);
    CHECK(transport.writes[0][6] == 0x02);

    identifier.setValue(50);
    transport.responses.push_back(reply(scopelink::command::DataIdentifier, Frame{ 0x01, 0x02, 0x02 }));

    CHECK(identifier.write(device));
    CHECK(transport.writes.size() == 2);
    CHECK(transport.writes[1][4] == 0x01); // write operation
    CHECK(transport.writes[1][7] == 0x00);
    CHECK(transport.writes[1][8] == 0x32); // 50, big endian

    // An answer for a different identifier is refused rather than stored under the wrong name.
    transport.responses.push_back(reply(scopelink::command::DataIdentifier, Frame{ 0x00, 0x02, 0x12, 0x00, 0x19 }));

    CHECK(!identifier.read(device));
    CHECK(!identifier.lastError().empty());

    // A value the controller would refuse never reaches the wire. The limit is the firmware's here - the
    // field itself would hold 60000 quite happily - so nothing is spent finding out that it is refused,
    // and what comes back says what the parameter does accept, in the units it is shown in.
    const size_t before = transport.writes.size();

    CHECK(identifier.maximum() == 5000);

    identifier.setValue(60000);
    CHECK(!identifier.write(device));
    CHECK(transport.writes.size() == before);
    CHECK(identifier.lastError().find("0.00 K to 100.00 K") != std::string::npos);
}

void testDidFileRoundTrip()
{
    std::vector<scopelink::Did> written = scopelink::buildDidCatalogue(capabilitiesFor(3));

    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    // Give every identifier a value and mark it available, which is what the exporter writes out.
    int seed = 1;

    for (scopelink::Did &identifier : written)
    {
        // The response has to be exactly as long as the identifier's type says, which is the check that
        // caught this test being written with one frame shape for every type.
        Frame payload = { 0x00, static_cast<uint8_t>(identifier.id() >> 8),
                          static_cast<uint8_t>(identifier.id() & 0xff) };

        payload.resize(3 + identifier.length(), 0);
        payload.back() = static_cast<uint8_t>(seed & 0x7f);

        transport.responses.push_back(reply(scopelink::command::DataIdentifier, payload));

        CHECK(identifier.read(device));
        CHECK(identifier.value() == (seed & 0x7f));

        seed++;
    }

    const std::string path = "scopelink-test-parameters.xml";

    scopelink::did_file::write(path, "HW 3.0, id TEST", written);

    std::vector<scopelink::Did> loaded = scopelink::buildDidCatalogue(capabilitiesFor(3));
    const size_t applied               = scopelink::did_file::read(path, loaded);

    CHECK(applied == written.size());

    bool identical = true;

    for (size_t index = 0; index < written.size(); index++)
        identical = identical && (written[index].value() == loaded[index].value());

    CHECK(identical);

    // A file from a generation 3 unit is still useful on a generation 2 one, for what the two share.
    std::vector<scopelink::Did> older = scopelink::buildDidCatalogue(capabilitiesFor(2));

    CHECK(scopelink::did_file::read(path, older) == older.size());

    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------------------------------
// Recorded vectors
// ---------------------------------------------------------------------------------------------------

#ifdef SCOPELINK_VECTOR_DIR

/**
 * @brief A transport that replays a conversation recorded from a real controller.
 *
 * A request is answered with the response the controller gave to that same request. Matching is on the
 * whole request first and on the command byte alone second, because one request - the clock set - carries
 * the moment of capture and can never match again.
 *
 * A request the recording has no answer for is reported rather than guessed at, and the read that follows
 * times out. That is the failure to want: it means the driver now asks the controller something the
 * recording predates, and the recording has to be taken again.
 */
class RecordedTransport : public scopelink::ISerialTransport
{
    public:
        struct Exchange
        {
                Frame request;
                Frame response;
                bool used{ false };
        };

        std::vector<Exchange> exchanges;
        std::vector<std::string> problems;

        bool isOpen() const override { return true; }
        void close() override {}
        void discardBuffers() override {}

        void write(const Frame &data) override
        {
            Exchange *match = find(data, true);

            if (match == nullptr)
                match = find(data, false);

            if (match == nullptr)
            {
                problems.push_back("nothing recorded for the request " + scopelink::toHex(data, data.size()));
                return;
            }

            match->used = true;
            m_pending.insert(m_pending.end(), match->response.begin(), match->response.end());
        }

        Frame read(size_t count) override
        {
            if (m_pending.size() < count)
                throw scopelink::TimeoutError("The recording has nothing more to answer with.");

            Frame chunk(m_pending.begin(), m_pending.begin() + static_cast<long>(count));

            m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<long>(count));

            return chunk;
        }

        bool reopen() override { return true; }
        std::string name() const override { return "recording"; }

    private:
        Exchange *find(const Frame &request, bool exact)
        {
            for (Exchange &exchange : exchanges)
            {
                if (exchange.used)
                    continue;

                if (exact)
                {
                    if (exchange.request == request)
                        return &exchange;
                }
                else if ((exchange.request.size() >= 3) && (request.size() >= 3) && (exchange.request[2] == request[2]))
                {
                    return &exchange;
                }
            }

            return nullptr;
        }

        Frame m_pending;
};

/** @brief Strips a comment, then leading and trailing blanks. */
std::string trimmed(const std::string &text)
{
    const size_t comment = text.find('#');
    std::string body     = (comment == std::string::npos) ? text : text.substr(0, comment);
    const size_t first   = body.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
        return "";

    return body.substr(first, body.find_last_not_of(" \t\r\n") - first + 1);
}

/** @brief Reads a line of space separated hex bytes. */
Frame parseHex(const std::string &text)
{
    Frame frame;
    std::istringstream stream(text);
    std::string token;

    while (stream >> token)
        frame.push_back(static_cast<uint8_t>(std::strtoul(token.c_str(), nullptr, 16)));

    return frame;
}

/** @brief Loads a .wire recording: "-> request" and "<- response" lines, in order. */
std::vector<RecordedTransport::Exchange> loadWire(const std::string &path)
{
    std::vector<RecordedTransport::Exchange> exchanges;
    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line))
    {
        const std::string body = trimmed(line);

        if (body.size() < 3)
            continue;

        if (body.compare(0, 2, "->") == 0)
        {
            RecordedTransport::Exchange exchange;

            exchange.request = parseHex(body.substr(2));
            exchanges.push_back(exchange);
        }
        else if ((body.compare(0, 2, "<-") == 0) && !exchanges.empty())
        {
            exchanges.back().response = parseHex(body.substr(2));
        }
    }

    return exchanges;
}

/**
 * @brief The values a recording is expected to decode to, and which of them have been looked at.
 *
 * Keys nobody asks about are reported at the end, so that a mistyped key in a .expected file fails the
 * test rather than passing quietly by never being compared to anything.
 */
class Expectations
{
    public:
        explicit Expectations(const std::string &path)
        {
            std::ifstream file(path);
            std::string line;

            while (std::getline(file, line))
            {
                const std::string body = trimmed(line);
                const size_t separator = body.find('=');

                if ((body.empty()) || (separator == std::string::npos))
                    continue;

                m_values[trimmed(body.substr(0, separator))] = trimmed(body.substr(separator + 1));
            }
        }

        bool empty() const { return m_values.empty(); }

        void expect(const std::string &key, const std::string &actual)
        {
            const auto found = m_values.find(key);

            g_checks++;

            if (found == m_values.end())
            {
                g_failures++;
                printf("  FAIL  %s is not in the expectations file\n", key.c_str());
                return;
            }

            m_asked.insert(key);

            if (found->second == actual)
                return;

            g_failures++;
            printf("  FAIL  %s: recorded frame decodes to \"%s\", expected \"%s\"\n", key.c_str(), actual.c_str(),
                   found->second.c_str());
        }

        void expect(const std::string &key, long actual) { expect(key, std::to_string(actual)); }
        void expect(const std::string &key, bool actual) { expect(key, std::string(actual ? "1" : "0")); }

        /** @brief Fails for every expectation the test never compared anything against. */
        void requireAllAsked()
        {
            for (const auto &entry : m_values)
            {
                g_checks++;

                if (m_asked.count(entry.first) > 0)
                    continue;

                g_failures++;
                printf("  FAIL  %s is expected but nothing in the test reads it\n", entry.first.c_str());
            }
        }

    private:
        std::map<std::string, std::string> m_values;
        std::set<std::string> m_asked;
};

/** @brief Value of one named freeze frame field, or a note that it is missing. */
std::string snapshotField(const scopelink::Fault &fault, const std::string &name)
{
    for (const scopelink::FaultSnapshotField &field : fault.snapshot)
    {
        if (field.name == name)
            return std::to_string(field.value);
    }

    return "(no such field)";
}

/** @brief Replays one recording through the real Device, and checks what it decoded. */
void replay(const char *stem, const std::function<void(scopelink::Device &, Expectations &)> &body)
{
    const std::string base = std::string(SCOPELINK_VECTOR_DIR) + "/" + stem;

    RecordedTransport transport;
    Expectations expectations(base + ".expected");

    transport.exchanges = loadWire(base + ".wire");

    CHECK(!transport.exchanges.empty());
    CHECK(!expectations.empty());

    if (transport.exchanges.empty() || expectations.empty())
        return;

    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    device.open();

    const scopelink::Identification &identification = device.identification();
    const scopelink::Capabilities &capabilities     = device.capabilities();

    expectations.expect("identification.hardwareMajor", static_cast<long>(identification.hardwareMajor));
    expectations.expect("identification.hardwareMinor", static_cast<long>(identification.hardwareMinor));
    expectations.expect("identification.interfaceMajor", static_cast<long>(identification.interfaceMajor));
    expectations.expect("identification.interfaceMinor", static_cast<long>(identification.interfaceMinor));
    expectations.expect("identification.hardwareIdentifier", identification.hardwareIdentifier);
    expectations.expect("identification.softwareIdentifier", identification.softwareIdentifier);

    expectations.expect("capabilities.statusFrameLength", static_cast<long>(capabilities.statusFrameLength));
    expectations.expect("capabilities.dtcSnapshotLength", static_cast<long>(capabilities.dtcSnapshotLength));
    expectations.expect("capabilities.hasUsbHub", capabilities.hasUsbHub);
    expectations.expect("capabilities.hasSmartSwitches", capabilities.hasSmartSwitchDiagnostics);
    expectations.expect("capabilities.hasTemperatureSensor", capabilities.hasTemperatureSensor);

    body(device, expectations);

    for (const std::string &problem : transport.problems)
    {
        g_checks++;
        g_failures++;
        printf("  FAIL  %s\n", problem.c_str());
    }

    expectations.requireAllAsked();
}

/**
 * @brief A whole session recorded from the generation 3 unit on the bench.
 *
 * This is the check the hand built frames cannot make: that the layouts the parsers are written against
 * are the layouts the firmware actually sends. The unit it came from has a working infrared sensor, so it
 * also covers the ambient and mirror readings, which no hand built frame can claim to have measured.
 */
void testRecordedSession()
{
    replay("gen3-com20-session",
           [](scopelink::Device &device, Expectations &expectations)
           {
               // The sample the open took, which is the first of the two recorded status frames. The
               // second belongs to Fault::readAll below, which reads the fault count out of its own fresh
               // status rather than trusting one taken earlier.
               const scopelink::Status &status = device.status();

               expectations.expect("status.supplyVoltage", static_cast<long>(status.supplyVoltage));
               expectations.expect("status.sensorSupplyVoltage", static_cast<long>(status.sensorSupplyVoltage));
               expectations.expect("status.fanAVoltage", static_cast<long>(status.fanAVoltage));
               expectations.expect("status.fanBVoltage", static_cast<long>(status.fanBVoltage));
               expectations.expect("status.controllerTemperature", static_cast<long>(status.controllerTemperature));
               expectations.expect("status.controllerSupplyVoltage", static_cast<long>(status.controllerSupplyVoltage));
               expectations.expect("status.fanATargetDT", static_cast<long>(status.fanATargetDT));
               expectations.expect("status.fanBTargetDT", static_cast<long>(status.fanBTargetDT));
               expectations.expect("status.ambientTemperatureRaw", static_cast<long>(status.ambientTemperatureRaw));
               expectations.expect("status.ambientTemperatureValid", status.ambientTemperatureValid);
               expectations.expect("status.mirrorTemperatureRaw", static_cast<long>(status.mirrorTemperatureRaw));
               expectations.expect("status.mirrorTemperatureValid", status.mirrorTemperatureValid);
               expectations.expect("status.motor1Position", static_cast<long>(status.motor(0).position));
               expectations.expect("status.motor1Moving", status.motor(0).moving);
               expectations.expect("status.motor1Load", static_cast<long>(status.motor(0).load));
               expectations.expect("status.motor2Position", static_cast<long>(status.motor(1).position));
               expectations.expect("status.motor2Moving", status.motor(1).moving);
               expectations.expect("status.flatboxDuty", static_cast<long>(status.flatboxDuty));
               expectations.expect("status.powerSwitch1State", status.powerSwitch1State);
               expectations.expect("status.powerSwitch2State", status.powerSwitch2State);
               expectations.expect("status.fanAManualOverrideEnabled", status.fanAManualOverrideEnabled);
               expectations.expect("status.fanBManualOverrideEnabled", status.fanBManualOverrideEnabled);
               expectations.expect("status.storedFaultCount", static_cast<long>(status.storedFaultCount));
               expectations.expect("status.activeFaultCount", static_cast<long>(status.activeFaultCount));
               expectations.expect("status.cpuLoad", static_cast<long>(status.cpuLoad));
               expectations.expect("status.peakCpuLoad", static_cast<long>(status.peakCpuLoad));
               expectations.expect("status.stackUsage", static_cast<long>(status.stackUsage));
               expectations.expect("status.i2cErrorCounter", static_cast<long>(status.i2cErrorCounter));
               expectations.expect("status.usb1PowerActive", status.usbPowerActive(0));
               expectations.expect("status.usb2PowerActive", status.usbPowerActive(1));
               expectations.expect("status.usb1PowerFailure", status.usb1PowerFailure);
               expectations.expect("status.usb2PowerFailure", status.usb2PowerFailure);

               const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

               expectations.expect("faults.count", static_cast<long>(faults.size()));

               if (faults.empty())
                   return;

               const scopelink::Fault &fault = faults.front();

               expectations.expect("faults.0.code", static_cast<long>(fault.code));
               expectations.expect("faults.0.name", fault.codeName());
               expectations.expect("faults.0.occurrenceCount", static_cast<long>(fault.occurrenceCount));
               expectations.expect("faults.0.isActive", fault.isActive);
               expectations.expect("faults.0.snapshotFieldCount", static_cast<long>(fault.snapshot.size()));
               expectations.expect("faults.0.timestamp", fault.timestamp());
               expectations.expect("faults.0.field.Supply voltage", snapshotField(fault, "Supply voltage"));
               expectations.expect("faults.0.field.RTC year", snapshotField(fault, "RTC year"));
               expectations.expect("faults.0.field.USB DS1 power active", snapshotField(fault, "USB DS1 power active"));
           });
}

/**
 * @brief The same session, recorded from the generation 2 unit.
 *
 * The pair is the point. One recording proves the parsers read that one unit; two, from generations whose
 * status and freeze frames are different lengths with their fields in different places, prove the driver
 * takes the layout from the identification block rather than from a length check both would pass.
 */
void testRecordedSessionGen2()
{
    replay("gen2-com6-session",
           [](scopelink::Device &device, Expectations &expectations)
           {
               // The sample the open took, which is the first of the two recorded status frames. The
               // second belongs to Fault::readAll below, which reads the fault count out of its own fresh
               // status rather than trusting one taken earlier.
               const scopelink::Status &status = device.status();

               expectations.expect("status.supplyVoltage", static_cast<long>(status.supplyVoltage));
               expectations.expect("status.sensorSupplyVoltage", static_cast<long>(status.sensorSupplyVoltage));
               expectations.expect("status.fanAVoltage", static_cast<long>(status.fanAVoltage));
               expectations.expect("status.fanBVoltage", static_cast<long>(status.fanBVoltage));
               expectations.expect("status.controllerTemperature", static_cast<long>(status.controllerTemperature));
               expectations.expect("status.controllerSupplyVoltage", static_cast<long>(status.controllerSupplyVoltage));
               expectations.expect("status.fanATargetDT", static_cast<long>(status.fanATargetDT));
               expectations.expect("status.fanBTargetDT", static_cast<long>(status.fanBTargetDT));
               expectations.expect("status.ambientTemperatureRaw", static_cast<long>(status.ambientTemperatureRaw));
               expectations.expect("status.ambientTemperatureValid", status.ambientTemperatureValid);
               expectations.expect("status.mirrorTemperatureRaw", static_cast<long>(status.mirrorTemperatureRaw));
               expectations.expect("status.mirrorTemperatureValid", status.mirrorTemperatureValid);
               expectations.expect("status.motor1Position", static_cast<long>(status.motor(0).position));
               expectations.expect("status.motor1Moving", status.motor(0).moving);
               expectations.expect("status.motor1Load", static_cast<long>(status.motor(0).load));
               expectations.expect("status.motor2Position", static_cast<long>(status.motor(1).position));
               expectations.expect("status.motor2Moving", status.motor(1).moving);
               expectations.expect("status.flatboxDuty", static_cast<long>(status.flatboxDuty));
               expectations.expect("status.powerSwitch1State", status.powerSwitch1State);
               expectations.expect("status.powerSwitch2State", status.powerSwitch2State);
               expectations.expect("status.fanAManualOverrideEnabled", status.fanAManualOverrideEnabled);
               expectations.expect("status.fanBManualOverrideEnabled", status.fanBManualOverrideEnabled);
               expectations.expect("status.storedFaultCount", static_cast<long>(status.storedFaultCount));
               expectations.expect("status.activeFaultCount", static_cast<long>(status.activeFaultCount));
               expectations.expect("status.cpuLoad", static_cast<long>(status.cpuLoad));
               expectations.expect("status.peakCpuLoad", static_cast<long>(status.peakCpuLoad));
               expectations.expect("status.stackUsage", static_cast<long>(status.stackUsage));
               expectations.expect("status.i2cErrorCounter", static_cast<long>(status.i2cErrorCounter));
               expectations.expect("status.usb1PowerActive", status.usbPowerActive(0));
               expectations.expect("status.usb2PowerActive", status.usbPowerActive(1));
               expectations.expect("status.usb1PowerFailure", status.usb1PowerFailure);
               expectations.expect("status.usb2PowerFailure", status.usb2PowerFailure);

               const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

               expectations.expect("faults.count", static_cast<long>(faults.size()));

               if (faults.empty())
                   return;

               const scopelink::Fault &fault = faults.front();

               expectations.expect("faults.0.code", static_cast<long>(fault.code));
               expectations.expect("faults.0.name", fault.codeName());
               expectations.expect("faults.0.occurrenceCount", static_cast<long>(fault.occurrenceCount));
               expectations.expect("faults.0.isActive", fault.isActive);
               expectations.expect("faults.0.snapshotFieldCount", static_cast<long>(fault.snapshot.size()));
               expectations.expect("faults.0.timestamp", fault.timestamp());
               expectations.expect("faults.0.field.Supply voltage", snapshotField(fault, "Supply voltage"));
               expectations.expect("faults.0.field.RTC year", snapshotField(fault, "RTC year"));
               expectations.expect("faults.0.field.USB DS1 power active", snapshotField(fault, "USB DS1 power active"));
           });
}

/** @brief The wear counters, whose field order is the part worth recording. */
void testRecordedEeprom()
{
    replay("gen3-com20-eeprom",
           [](scopelink::Device &device, Expectations &expectations)
           {
               const scopelink::EepromStatistics eeprom = scopelink::EepromStatistics::read(device);

               expectations.expect("eeprom.pageEraseCounter", static_cast<long>(eeprom.pageEraseCounter));
               expectations.expect("eeprom.datasetCounter", static_cast<long>(eeprom.datasetCounter));
               expectations.expect("eeprom.learntDataCounter", static_cast<long>(eeprom.learntDataCounter));
               expectations.expect("eeprom.faultStoreBlock1Counter", static_cast<long>(eeprom.faultStoreBlock1Counter));
               expectations.expect("eeprom.faultStoreBlock2Counter", static_cast<long>(eeprom.faultStoreBlock2Counter));
               expectations.expect("eeprom.faultStoreBlock3Counter", static_cast<long>(eeprom.faultStoreBlock3Counter));
               expectations.expect("eeprom.faultStoreBlock4Counter", static_cast<long>(eeprom.faultStoreBlock4Counter));
           });
}

/** @brief The wear counters of the generation 2 unit, which has never had a page erased. */
void testRecordedEepromGen2()
{
    replay("gen2-com6-eeprom",
           [](scopelink::Device &device, Expectations &expectations)
           {
               const scopelink::EepromStatistics eeprom = scopelink::EepromStatistics::read(device);

               expectations.expect("eeprom.pageEraseCounter", static_cast<long>(eeprom.pageEraseCounter));
               expectations.expect("eeprom.datasetCounter", static_cast<long>(eeprom.datasetCounter));
               expectations.expect("eeprom.learntDataCounter", static_cast<long>(eeprom.learntDataCounter));
               expectations.expect("eeprom.faultStoreBlock1Counter", static_cast<long>(eeprom.faultStoreBlock1Counter));
               expectations.expect("eeprom.faultStoreBlock2Counter", static_cast<long>(eeprom.faultStoreBlock2Counter));
               expectations.expect("eeprom.faultStoreBlock3Counter", static_cast<long>(eeprom.faultStoreBlock3Counter));
               expectations.expect("eeprom.faultStoreBlock4Counter", static_cast<long>(eeprom.faultStoreBlock4Counter));
           });
}

#endif

// ---------------------------------------------------------------------------------------------------
// Firmware update
// ---------------------------------------------------------------------------------------------------

/**
 * @brief Builds a boot info payload, written against boot_diag.c rather than against the parser.
 *
 * The offsets below are the order BootDiag_GetBootInfo() writes its fields in. Getting one of them wrong
 * in both this and the parser at once is the failure this cannot catch and nothing else would either,
 * which is why they are laid out here field by field with the firmware's own names beside them.
 */
Frame bootInfoPayload(int protocolVersion, unsigned slotSize, unsigned pageSize, unsigned pageCount,
                      unsigned chunkSize, bool startable, bool readProtected, bool reportProtection = true)
{
    Frame payload;

    payload.push_back(1);                                    //  0 in boot loader
    payload.push_back(static_cast<uint8_t>(protocolVersion)); //  1 BOOT_DIAG_PROTOCOL_VERSION

    put32(payload, 0x08006000);      //  2 MM_DRIVE_ADDR
    put32(payload, slotSize);        //  6 MM_DRIVE_SIZE
    put16(payload, pageSize);        // 10 MM_FLASH_PAGE_SIZE
    put16(payload, pageCount);       // 12 MM_DRIVE_PAGE_COUNT
    put16(payload, chunkSize);       // 14 BOOT_DL_MAX_DATA_LENGTH

    payload.push_back(startable ? 1 : 0); // 16 IsDriveSWPresent()

    put32(payload, 6000);       // 17 descriptor size
    put32(payload, 0x52b8f7aa); // 21 descriptor checksum

    if (reportProtection)
        payload.push_back(readProtected ? 1 : 0); // 25 BootFlash_IsReadProtected()

    return payload;
}

void testBootInfoParsing()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::BootInfo info;

    transport.responses.push_back(
        reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 32768, 2048, 16, 240, true, true)));

    CHECK(scopelink::BootInfo::read(protocol, info));
    CHECK(info.protocolVersion == 5);
    CHECK(info.slotAddress == 0x08006000);
    CHECK(info.slotSize == 32768);
    CHECK(info.pageSize == 2048);
    CHECK(info.pageCount == 16);
    CHECK(info.chunkSize == 240);
    CHECK(info.hasStartableFirmware);
    CHECK(info.storedImageSize == 6000);
    CHECK(info.storedImageChecksum == 0x52b8f7aa);
    CHECK(info.reportsReadProtection);
    CHECK(info.isReadProtected);

    // A boot loader from before the read protection was reported is one byte shorter and still readable.
    MockTransport older;
    scopelink::Protocol olderProtocol(older);
    scopelink::BootInfo olderInfo;

    older.responses.push_back(
        reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 6144, 1024, 6, 240, false, false, false)));

    CHECK(scopelink::BootInfo::read(olderProtocol, olderInfo));
    CHECK(olderInfo.slotSize == 6144);
    CHECK(!olderInfo.reportsReadProtection);
    CHECK(!olderInfo.hasStartableFirmware);

    // Silence is how the firmware answers this identifier, because it has no handler for it at all.
    MockTransport silent;
    scopelink::Protocol silentProtocol(silent);
    scopelink::BootInfo unused;

    silent.reopenSucceeds = false;

    CHECK(!scopelink::BootInfo::read(silentProtocol, unused));

    // So is a single zero byte, which is the shape a refusal takes.
    MockTransport refusing;
    scopelink::Protocol refusingProtocol(refusing);

    refusing.responses.push_back(reply(scopelink::boot::GetBootInfo, Frame{ 0 }));

    CHECK(!scopelink::BootInfo::read(refusingProtocol, unused));

    // An answer that is neither is a boot loader this driver cannot drive, and has to say so rather than
    // be taken for a firmware and quietly waited out.
    MockTransport truncated;
    scopelink::Protocol truncatedProtocol(truncated);

    truncated.responses.push_back(reply(scopelink::boot::GetBootInfo, Frame(10, 1)));

    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::BootInfo::read(truncatedProtocol, unused); }));
}

void testBootInfoSupport()
{
    scopelink::BootInfo info;

    info.protocolVersion = scopelink::boot::ProtocolVersion;
    info.slotSize        = 32768;
    info.pageSize        = 2048;
    info.chunkSize       = 240;

    info.requireSupported();

    // The service identifiers moved when the interface reached version 5, so an older boot loader answers
    // a different identifier for the very request that reports the version. Falling back is therefore not
    // an option and the mismatch has to be refused outright.
    scopelink::BootInfo older = info;

    older.protocolVersion = 4;
    CHECK(throws<scopelink::FirmwareError>([&] { older.requireSupported(); }));

    // A transfer size that is not a whole number of cipher blocks cannot be decrypted at the far end.
    scopelink::BootInfo odd = info;

    odd.chunkSize = 250;
    CHECK(throws<scopelink::FirmwareError>([&] { odd.requireSupported(); }));

    scopelink::BootInfo empty = info;

    empty.slotSize = 0;
    CHECK(throws<scopelink::FirmwareError>([&] { empty.requireSupported(); }));
}

/** @brief Writes a file of @p length bytes and returns its path. */
std::string writeTemporary(const std::string &name, size_t length)
{
    const std::string path = std::string("/tmp/") + name;
    std::ofstream stream(path, std::ios::binary);

    for (size_t index = 0; index < length; index++)
        stream.put(static_cast<char>(index & 0xff));

    return path;
}

/**
 * @brief Rewrites a file at a length of its own, keeping the name.
 *
 * The name is part of what a firmware file is checked on, so a test that needs the same file at two
 * different lengths cannot simply use two names.
 */
void rewriteTemporary(const std::string &path, size_t length)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);

    for (size_t index = 0; index < length; index++)
        stream.put(static_cast<char>(index & 0xff));
}

void testFirmwareFileChecks()
{
    const std::string identifier = "DEADBEEF0102030405060708";

    // Nothing here decrypts anything: the driver holds no key and the boot loader is the authority on
    // what a container contains. These are the checks the shape of the file gives away for nothing.
    const std::string good = writeTemporary(identifier + ".bin", 6160);
    scopelink::FirmwareFile file = scopelink::FirmwareFile::open(good, identifier);

    CHECK(file.container().size() == 6160);
    CHECK(file.name() == (identifier + ".bin"));

    // The name is the only thing about the intended target a driver without the key can read, so a file
    // that does not carry it is refused rather than handed to the controller to discover after the erase.
    const std::string wrongName = writeTemporary("somebody-elses-firmware.bin", 6160);

    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::FirmwareFile::open(wrongName, identifier); }));
    CHECK(scopelink::FirmwareFile::nameMatchesController(good, identifier));
    CHECK(scopelink::FirmwareFile::nameMatchesController("/tmp/deadbeef0102030405060708.bin", identifier));
    CHECK(!scopelink::FirmwareFile::nameMatchesController(wrongName, identifier));

    // A container is always a whole number of cipher blocks, and always longer than a header.
    const std::string ragged = writeTemporary(identifier + ".bin.ragged", 6155);
    const std::string tiny   = writeTemporary(identifier + ".bin.tiny", 16);

    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::FirmwareFile::open(ragged, identifier); }));
    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::FirmwareFile::open(tiny, identifier); }));
    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::FirmwareFile::open("/tmp/not-here.bin", identifier); }));

    // A controller that reported no identifier cannot have a file checked against it at all.
    CHECK(throws<scopelink::FirmwareError>([&] { scopelink::FirmwareFile::open(good, ""); }));

    scopelink::BootInfo big;
    scopelink::BootInfo small;

    big.slotSize   = 32768;
    small.slotSize = 6144;

    file.requireFitsSlot(big);

    // 6144 bytes of slot plus the one block header is 6160, so this file is exactly the largest a small
    // part can take. The boundary is checked rather than assumed because the boot loader draws it in the
    // same place - BOOT_DL_MAX_CONTAINER_SIZE is MM_DRIVE_SIZE plus IMAGE_HEADER_SIZE - and a driver that
    // stopped one block short would refuse the one container that fills the part exactly.
    file.requireFitsSlot(small);

    // One block more than that was packaged for a larger part. Refused before the erase rather than
    // after it: the boot loader would refuse it too, but only once the working firmware was gone.
    rewriteTemporary(good, 6176);

    scopelink::FirmwareFile oversized = scopelink::FirmwareFile::open(good, identifier);

    CHECK(throws<scopelink::FirmwareError>([&] { oversized.requireFitsSlot(small); }));
    oversized.requireFitsSlot(big);
}

/** @brief Runs the update until it stops, pacing the calls the way a driver's timer would. */
void runUpdate(scopelink::FirmwareUpdate &update, int maximumSteps = 400)
{
    for (int step = 0; (step < maximumSteps) && update.step(); step++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void testFirmwareUpdateSequence()
{
    const std::string identifier = "DEADBEEF0102030405060708";
    const size_t containerLength = 6160;
    const size_t chunkLength     = 240;

    MockTransport transport;

    transport.responses.push_back(
        reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 32768, 2048, 16, chunkLength, true, true)));
    transport.responses.push_back(reply(scopelink::boot::EraseSlot, Frame{ scopelink::boot::ResultOk }));

    const size_t pieces = (containerLength + chunkLength - 1) / chunkLength;

    for (size_t piece = 0; piece < pieces; piece++)
        transport.responses.push_back(reply(scopelink::boot::WriteData, Frame{ scopelink::boot::ResultOk }));

    Frame finished{ scopelink::boot::ResultOk };

    put32(finished, 0x52b8f7aa);
    transport.responses.push_back(reply(scopelink::boot::Finish, finished));
    transport.responses.push_back(reply(scopelink::boot::StartDrive, Frame{ scopelink::boot::ResultOk }));

    // What the controller answers once it has restarted into the firmware that was just written.
    scriptOpen(transport, 3);

    scopelink::FirmwareUpdate update(
        transport, scopelink::FirmwareFile::open(writeTemporary(identifier + ".bin", containerLength), identifier));

    runUpdate(update);

    CHECK(update.stage() == scopelink::FirmwareUpdate::Stage::Complete);
    CHECK(update.percent() == 100);
    CHECK(update.failure().empty());
    CHECK(update.device().identification().hardwareIdentifier == identifier);

    // Every piece of the container, in order, at the offset the boot loader expects it. The chaining
    // vector is carried from one request to the next on the controller, so an offset that is out by one
    // piece is not a slow transfer, it is a transfer that cannot be finished at all.
    size_t offset  = 0;
    size_t written = 0;

    for (const Frame &request : transport.writes)
    {
        if (request[2] != scopelink::boot::WriteData)
            continue;

        const size_t payload = request.size() - scopelink::Protocol::HeaderLength;
        const size_t claimed = (static_cast<size_t>(request[4]) << 24) | (static_cast<size_t>(request[5]) << 16)
                               | (static_cast<size_t>(request[6]) << 8) | request[7];

        CHECK(claimed == offset);
        CHECK((payload - 4) <= chunkLength);
        CHECK(((payload - 4) % scopelink::FirmwareFile::BlockLength) == 0);

        offset += payload - 4;
        written++;
    }

    CHECK(written == pieces);
    CHECK(offset == containerLength);

    // The erase stalls the controller until every page is done and the closing checksum is taken over the
    // whole slot, so both are given room the ordinary transfers are not - and the ordinary timeout is put
    // back afterwards, because leaving it there would make one dropped frame cost half a minute.
    bool erasePatience  = false;
    bool finishPatience = false;

    for (const int timeout : transport.timeouts)
    {
        erasePatience  = erasePatience || (timeout == scopelink::FirmwareUpdate::EraseTimeoutMs);
        finishPatience = finishPatience || (timeout == scopelink::FirmwareUpdate::FinishTimeoutMs);
    }

    CHECK(erasePatience);
    CHECK(finishPatience);
    CHECK(transport.timeouts.back() == scopelink::Device::ReceiveTimeoutMs);
}

void testFirmwareUpdateFailures()
{
    const std::string identifier = "DEADBEEF0102030405060708";
    const std::string path       = writeTemporary(identifier + ".bin", 6160);

    // A boot loader whose interface this driver does not speak, refused while the controller still holds
    // its working firmware.
    {
        MockTransport transport;

        transport.responses.push_back(
            reply(scopelink::boot::GetBootInfo, bootInfoPayload(4, 32768, 2048, 16, 240, true, true)));

        scopelink::FirmwareUpdate update(transport, scopelink::FirmwareFile::open(path, identifier));

        runUpdate(update);

        CHECK(update.stage() == scopelink::FirmwareUpdate::Stage::Failed);
        CHECK(update.failure().find("version 4") != std::string::npos);

        // Nothing was sent but the enquiry itself: the erase is the first thing that changes anything.
        for (const Frame &request : transport.writes)
            CHECK(request[2] == scopelink::boot::GetBootInfo);
    }

    // A container packaged for a larger part, likewise refused before the erase.
    {
        const std::string oversized = writeTemporary(identifier + ".bin", 20000);

        MockTransport transport;

        transport.responses.push_back(
            reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 6144, 1024, 6, 240, true, true)));

        scopelink::FirmwareUpdate update(transport, scopelink::FirmwareFile::open(oversized, identifier));

        runUpdate(update);

        CHECK(update.stage() == scopelink::FirmwareUpdate::Stage::Failed);
        CHECK(update.failure().find("6160") != std::string::npos);

        for (const Frame &request : transport.writes)
            CHECK(request[2] == scopelink::boot::GetBootInfo);

        // Every file here has to carry the unit's name, so they are all the same file; the cases below
        // count pieces and need it back at the length they were written for.
        rewriteTemporary(path, 6160);
    }

    // A piece that arrives where the boot loader is not expecting it. The chaining makes that
    // unrecoverable, so the message has to say the transfer must start again rather than blame the data.
    {
        MockTransport transport;

        transport.responses.push_back(
            reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 32768, 2048, 16, 240, true, true)));
        transport.responses.push_back(reply(scopelink::boot::EraseSlot, Frame{ scopelink::boot::ResultOk }));
        transport.responses.push_back(reply(scopelink::boot::WriteData, Frame{ scopelink::boot::ResultOk }));
        transport.responses.push_back(reply(scopelink::boot::WriteData, Frame{ scopelink::boot::ResultSequenceError }));

        scopelink::FirmwareUpdate update(transport, scopelink::FirmwareFile::open(path, identifier));

        runUpdate(update);

        CHECK(update.stage() == scopelink::FirmwareUpdate::Stage::Failed);
        CHECK(update.failure().find("lost its place") != std::string::npos);
        CHECK(update.failure().find("240") != std::string::npos);
    }

    // A container that arrived damaged. The boot loader is the one that finds it, by reading back the
    // flash it programmed, and it reports what it calculated so the two can be told apart.
    {
        MockTransport transport;

        transport.responses.push_back(
            reply(scopelink::boot::GetBootInfo, bootInfoPayload(5, 32768, 2048, 16, 240, true, true)));
        transport.responses.push_back(reply(scopelink::boot::EraseSlot, Frame{ scopelink::boot::ResultOk }));

        for (size_t piece = 0; piece < 26; piece++)
            transport.responses.push_back(reply(scopelink::boot::WriteData, Frame{ scopelink::boot::ResultOk }));

        Frame refused{ scopelink::boot::ResultCrcError };

        put32(refused, 0x12345678);
        transport.responses.push_back(reply(scopelink::boot::Finish, refused));

        scopelink::FirmwareUpdate update(transport, scopelink::FirmwareFile::open(path, identifier));

        runUpdate(update);

        CHECK(update.stage() == scopelink::FirmwareUpdate::Stage::Failed);
        CHECK(update.failure().find("checksum test") != std::string::npos);
        CHECK(update.failure().find("0x12345678") != std::string::npos);

        // The firmware was not started, and saying so matters: the controller is sitting in its boot
        // loader waiting to be sent the file again, which is a recoverable state rather than a dead unit.
        CHECK(update.failure().find("not been started") != std::string::npos);
    }
}

struct Test
{
        const char *name;
        void (*body)();
};

const Test tests[] = {
    { "byte order", testByteOrder },
    { "capabilities", testCapabilities },
    { "status parsing", testStatusParsing },
    { "temperature plausibility", testTemperaturePlausibility },
    { "protocol framing", testProtocolFraming },
    { "protocol resynchronisation", testProtocolResync },
    { "protocol command echo", testProtocolWrongCommandEcho },
    { "protocol retry", testProtocolRetryAndRecovery },
    { "protocol gives up on a dead port", testProtocolAbandonsUnrecoverablePort },
    { "protocol variable length", testProtocolVariableLength },
    { "device open", testDeviceOpen },
    { "fault decoding", testFaultDecoding },
    { "fault numbering", testFaultNumbering },
    { "simulated controllers", testSimulatedControllers },
    { "motor role rules", testMotorRoleRules },
    { "motor roles", testMotorRoles },
    { "function commands", testFunctionCommands },
    { "simulated function commands", testSimulatedFunctions },
    { "narrowed motor selections", testNarrowedMotorSelections },
    { "generated catalogue", testGeneratedCatalogue },
    { "DID catalogue", testDidCatalogue },
    { "DID ranges", testDidRanges },
    { "DID transactions", testDidTransactions },
    { "parameter file round trip", testDidFileRoundTrip },
    { "boot info parsing", testBootInfoParsing },
    { "boot loader interface support", testBootInfoSupport },
    { "firmware file checks", testFirmwareFileChecks },
    { "firmware update sequence", testFirmwareUpdateSequence },
    { "firmware update failures", testFirmwareUpdateFailures },
#ifdef SCOPELINK_VECTOR_DIR
    { "recorded session, generation 3", testRecordedSession },
    { "recorded wear counters, generation 3", testRecordedEeprom },
    { "recorded session, generation 2", testRecordedSessionGen2 },
    { "recorded wear counters, generation 2", testRecordedEepromGen2 },
#endif
};

} // namespace

int main()
{
    for (const Test &test : tests)
    {
        const int before = g_failures;

        printf("%-42s", test.name);

        // A test that throws is a failure of that test, not the end of the run. An escaping exception
        // used to abort the binary, which loses the name of the test that threw and every test behind
        // it - and a frame the driver decides it cannot parse is exactly the kind of defect that
        // throws rather than quietly returning the wrong number.
        std::string thrown;

        try
        {
            test.body();
        }
        catch (const std::exception &error)
        {
            g_failures++;
            thrown = error.what();
        }

        printf("%s\n", (g_failures == before) ? "ok" : "FAILED");

        if (!thrown.empty())
            printf("%-42s  threw: %s\n", "", thrown.c_str());
    }

    printf("\n%d checks, %d failure(s).\n", g_checks, g_failures);

    return (g_failures == 0) ? 0 : 1;
}
