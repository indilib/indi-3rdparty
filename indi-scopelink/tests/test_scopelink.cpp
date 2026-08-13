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
#include "scopelink/parameters.h"
#include "scopelink/protocol.h"
#include "scopelink/transport.h"

#include <cstdio>
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
            return reopenSucceeds;
        }

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
 */
Frame statusPayload(int hardwareMajor)
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

    payload.push_back(1);  // 30 focuser status, counter clockwise
    payload.push_back(42); // 31 focuser load
    put32(payload, 33000); // 32 focuser position

    payload.push_back(2);  // 36 flap status, halted
    payload.push_back(17); // 37 flap load
    put32(payload, 6000);  // 38 flap position

    payload.push_back(1); // 42 aux output 1
    payload.push_back(0); // 43 aux output 2

    put32(payload, 0); // 44 reserved

    payload.push_back(60); // 48 flat box duty
    payload.push_back(11); // 49
    payload.push_back(22); // 50
    payload.push_back(33); // 51
    payload.push_back(2);  // 52 stored faults
    payload.push_back(1);  // 53 active faults
    put16(payload, 7);     // 54 I2C errors

    if (hardwareMajor > 2)
    {
        payload.push_back(1); // 56
        payload.push_back(0); // 57
        payload.push_back(0); // 58
        payload.push_back(1); // 59
    }

    return payload;
}

scopelink::Capabilities capabilitiesFor(int hardwareMajor, int temperatureSensorFitted = -1)
{
    scopelink::Identification identification;

    identification.hardwareMajor = hardwareMajor;

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
    for (int generation = 2; generation <= 3; generation++)
    {
        const scopelink::Capabilities capabilities = capabilitiesFor(generation);
        const Frame frame                          = reply(scopelink::command::Status, statusPayload(generation));

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

        CHECK(status.motor1Moving);
        CHECK(status.motor1Direction == scopelink::MotorDirection::CounterClockwise);
        CHECK(status.motor1Load == 42);
        CHECK(status.motor1Position == 33000);

        CHECK(status.flatboxDuty == 60);
        CHECK(status.cpuLoad == 11);
        CHECK(status.peakCpuLoad == 22);
        CHECK(status.stackUsage == 33);
        CHECK(status.storedFaultCount == 2);
        CHECK(status.activeFaultCount == 1);
        CHECK(status.i2cErrorCounter == 7);

        CHECK(!status.motor2Moving);
        CHECK(status.motor2Direction == scopelink::MotorDirection::Halted);
        CHECK(status.motor2Load == 17);
        CHECK(status.motor2Position == 6000);
        CHECK(status.powerSwitch1State);
        CHECK(!status.powerSwitch2State);

        if (generation > 2)
        {
            CHECK(status.usb1PowerActive);
            CHECK(!status.usb2PowerActive);
            CHECK(!status.usb1PowerFailure);
            CHECK(status.usb2PowerFailure);
        }
    }

    // A frame of the wrong length for the generation is refused rather than decoded with the offsets of
    // whichever generation it happens to be long enough for.
    CHECK(throws<scopelink::ProtocolError>(
        [] { scopelink::Status::parse(reply(scopelink::command::Status, statusPayload(2)), capabilitiesFor(3)); }));
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

/** @brief Queues the exact exchange Device::open() performs, for a given hardware generation. */
void scriptOpen(MockTransport &transport, int generation)
{
    transport.responses.push_back(
        reply(scopelink::command::InterfaceVersion, Frame{ static_cast<uint8_t>(generation), 0, 1, 0 }));
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
    transport.responses.push_back(reply(scopelink::command::Status, statusPayload(generation)));
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
    CHECK(device.status().motor1Position == 33000);

    // A controller that will not take the clock is not a ScopeLink, which is where that check earns its
    // keep: it is the first command whose answer is specific rather than merely well formed.
    MockTransport wrong;
    scopelink::Protocol wrongProtocol(wrong);
    scopelink::Device wrongDevice(wrongProtocol);

    scriptOpen(wrong, 3);
    wrong.responses[4] = reply(scopelink::command::SetRtc, Frame{ 0 });

    CHECK(throws<scopelink::ProtocolError>([&] { wrongDevice.open(); }));
}

void testFaultDecoding()
{
    for (int generation = 2; generation <= 3; generation++)
    {
        const scopelink::Capabilities capabilities = capabilitiesFor(generation);

        MockTransport transport;
        scopelink::Protocol protocol(transport);
        scopelink::Device device(protocol);

        scriptOpen(transport, generation);
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

        payload.insert(payload.end(), snapshot.begin(), snapshot.end());

        // readAll() takes its own status sample first, to learn how many faults are stored.
        transport.responses.push_back(reply(scopelink::command::Status, statusPayload(generation)));
        transport.responses.push_back(reply(scopelink::command::ReadDtc, payload));

        const std::vector<scopelink::Fault> faults = scopelink::Fault::readAll(device);

        CHECK(faults.size() == 1);

        if (faults.empty())
            continue;

        CHECK(faults[0].code == scopelink::FaultCode::Mlx90614CommunicationTimeout);
        CHECK(faults[0].occurrenceCount == 3);
        CHECK(faults[0].isActive);
        CHECK(faults[0].timestamp() == "2026-08-11 21:45:30");
        CHECK(faults[0].codeName().find("Infrared") != std::string::npos);

        // The field count is the visible difference between the generations: ten smart switch and USB
        // fields and two auxiliary voltages that only generation 3 records.
        CHECK(faults[0].snapshot.size() == ((generation > 2) ? 36u : 24u));
    }
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
}

void testDidRanges()
{
    CHECK(scopelink::Did::minimumOf(scopelink::DidType::UInt8) == 0);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::UInt8) == 255);
    CHECK(scopelink::Did::minimumOf(scopelink::DidType::SInt8) == -128);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::SInt8) == 127);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::UInt16) == 65535);
    CHECK(scopelink::Did::maximumOf(scopelink::DidType::SInt16) == 32767);

    scopelink::Did identifier(0x000f, scopelink::DidType::UInt32, scopelink::DidGroup::FocuserMotor, "Travel");

    CHECK(identifier.elementName() == "DID_000F");
    CHECK(identifier.length() == 4);
    CHECK(identifier.isInRange(40000));
    CHECK(!identifier.isInRange(-1));
}

void testDidTransactions()
{
    MockTransport transport;
    scopelink::Protocol protocol(transport);
    scopelink::Device device(protocol);

    scopelink::Did identifier(0x0202, scopelink::DidType::UInt16, scopelink::DidGroup::Fans, "Rear fan target dT");

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

    // A value the type cannot hold never reaches the wire.
    const size_t before = transport.writes.size();

    identifier.setValue(100000);
    CHECK(!identifier.write(device));
    CHECK(transport.writes.size() == before);
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
               expectations.expect("status.motor1Position", static_cast<long>(status.motor1Position));
               expectations.expect("status.motor1Moving", status.motor1Moving);
               expectations.expect("status.motor1Load", static_cast<long>(status.motor1Load));
               expectations.expect("status.motor2Position", static_cast<long>(status.motor2Position));
               expectations.expect("status.motor2Moving", status.motor2Moving);
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
               expectations.expect("status.usb1PowerActive", status.usb1PowerActive);
               expectations.expect("status.usb2PowerActive", status.usb2PowerActive);
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
               expectations.expect("status.motor1Position", static_cast<long>(status.motor1Position));
               expectations.expect("status.motor1Moving", status.motor1Moving);
               expectations.expect("status.motor1Load", static_cast<long>(status.motor1Load));
               expectations.expect("status.motor2Position", static_cast<long>(status.motor2Position));
               expectations.expect("status.motor2Moving", status.motor2Moving);
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
               expectations.expect("status.usb1PowerActive", status.usb1PowerActive);
               expectations.expect("status.usb2PowerActive", status.usb2PowerActive);
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
    { "DID catalogue", testDidCatalogue },
    { "DID ranges", testDidRanges },
    { "DID transactions", testDidTransactions },
    { "parameter file round trip", testDidFileRoundTrip },
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
        test.body();
        printf("%s\n", (g_failures == before) ? "ok" : "FAILED");
    }

    printf("\n%d checks, %d failure(s).\n", g_checks, g_failures);

    return (g_failures == 0) ? 0 : 1;
}
