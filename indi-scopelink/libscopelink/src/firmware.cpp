/*
    ScopeLink INDI driver - firmware download to the controller's own boot loader

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink/firmware.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <utility>

namespace scopelink
{

/** @brief Formats a 32 bit value the way every checksum in a log line is written. */
static std::string hex32(uint32_t value)
{
    char text[16];

    snprintf(text, sizeof(text), "0x%08X", value);

    return text;
}

/** @brief The file name part of a path, without any directory in front of it. */
static std::string baseName(const std::string &path)
{
    const size_t slash = path.find_last_of('/');

    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

// ---------------------------------------------------------------------------------------------------
// Boot loader services
// ---------------------------------------------------------------------------------------------------

namespace boot
{

std::string describeResult(uint8_t result)
{
    switch (result)
    {
        case ResultOk:
            return "accepted";

        case ResultFlashError:
            return "the controller could not write its flash memory";

        case ResultInvalidRequest:
            return "the boot loader rejected the request";

        case ResultSequenceError:
            return "the request arrived out of order";

        case ResultCrcError:
            return "the downloaded firmware failed its checksum test";

        case ResultNoImage:
            return "there is no startable firmware in the controller";

        default:
            return "unknown result code " + std::to_string(result);
    }
}

/** @brief Turns a result code other than OK into an exception. */
static void check(const char *what, const Frame &response)
{
    const uint8_t result = byte_order::toByte(response, Protocol::HeaderLength);

    if (result != ResultOk)
        throw FirmwareError(std::string("The controller would not ") + what + ": " + describeResult(result) + ".");
}

} // namespace boot

// ---------------------------------------------------------------------------------------------------
// Boot info
// ---------------------------------------------------------------------------------------------------

bool BootInfo::read(Protocol &protocol, BootInfo &info)
{
    Frame frame;

    try
    {
        frame = protocol.transactVariable(boot::GetBootInfo, {}, Protocol::HeaderLength + 1);
    }
    catch (const CommunicationError &)
    {
        // The firmware has no handler for this identifier and says nothing at all, so silence is the
        // expected answer from a controller that is running rather than a fault.
        return false;
    }

    const size_t payloadLength = frame.size() - Protocol::HeaderLength;

    // A single zero byte is the shape the boot loader itself uses to refuse a malformed request, and is
    // what a firmware that one day grows a handler for this identifier is specified to answer.
    if ((payloadLength == 1) && (byte_order::toByte(frame, Protocol::HeaderLength) == 0))
        return false;

    if (payloadLength < MinimumPayloadLength)
    {
        throw FirmwareError("The controller answered the boot loader enquiry with " + std::to_string(payloadLength)
                            + " bytes and at least " + std::to_string(MinimumPayloadLength)
                            + " are needed. This is not a ScopeLink boot loader this driver can drive.");
    }

    const size_t base = Protocol::HeaderLength;

    info                       = BootInfo();
    info.protocolVersion       = byte_order::toByte(frame, base + 1);
    info.slotAddress           = byte_order::toUInt(frame, base + 2, 4);
    info.slotSize              = byte_order::toUInt(frame, base + 6, 4);
    info.pageSize              = static_cast<int>(byte_order::toUInt(frame, base + 10, 2));
    info.pageCount             = static_cast<int>(byte_order::toUInt(frame, base + 12, 2));
    info.chunkSize             = static_cast<int>(byte_order::toUInt(frame, base + 14, 2));
    info.hasStartableFirmware  = byte_order::toBool(frame, base + 16);
    info.storedImageSize       = byte_order::toUInt(frame, base + 17, 4);
    info.storedImageChecksum   = byte_order::toUInt(frame, base + 21, 4);
    info.reportsReadProtection = payloadLength >= FullPayloadLength;
    info.isReadProtected       = info.reportsReadProtection && byte_order::toBool(frame, base + 25);

    return true;
}

void BootInfo::requireSupported() const
{
    if (protocolVersion != boot::ProtocolVersion)
    {
        throw FirmwareError("The ScopeLink boot loader speaks version " + std::to_string(protocolVersion)
                            + " of the update interface and this driver speaks version "
                            + std::to_string(boot::ProtocolVersion) + ". Install the driver version that matches "
                            + "the controller.");
    }

    if ((chunkSize <= 0) || ((static_cast<size_t>(chunkSize) % FirmwareFile::BlockLength) != 0))
    {
        throw FirmwareError("The ScopeLink boot loader asks for the firmware in pieces of "
                            + std::to_string(chunkSize) + " bytes, which is not a whole number of "
                            + std::to_string(FirmwareFile::BlockLength) + " byte cipher blocks.");
    }

    if ((slotSize == 0) || (pageSize <= 0))
    {
        throw FirmwareError("The ScopeLink boot loader describes its firmware slot as " + std::to_string(slotSize)
                            + " bytes in pages of " + std::to_string(pageSize) + ", which cannot be right.");
    }
}

std::string BootInfo::toString() const
{
    char text[192];

    snprintf(text, sizeof(text), "interface version %d, firmware slot 0x%08X, %u bytes in %d page(s) of %d, %d bytes "
                                 "per transfer",
             protocolVersion, slotAddress, slotSize, pageCount, pageSize, chunkSize);

    return text;
}

// ---------------------------------------------------------------------------------------------------
// The file
// ---------------------------------------------------------------------------------------------------

FirmwareFile::FirmwareFile(std::string path, Frame container)
    : m_path(std::move(path)), m_container(std::move(container))
{
}

std::string FirmwareFile::name() const
{
    return baseName(m_path);
}

bool FirmwareFile::nameMatchesController(const std::string &path, const std::string &hardwareIdentifier)
{
    if (hardwareIdentifier.empty())
        return false;

    std::string name = baseName(path);
    const size_t dot = name.find_last_of('.');

    if (dot != std::string::npos)
        name = name.substr(0, dot);

    if (name.size() != hardwareIdentifier.size())
        return false;

    for (size_t index = 0; index < name.size(); index++)
    {
        if (std::toupper(static_cast<unsigned char>(name[index]))
            != std::toupper(static_cast<unsigned char>(hardwareIdentifier[index])))
        {
            return false;
        }
    }

    return true;
}

FirmwareFile FirmwareFile::open(const std::string &path, const std::string &hardwareIdentifier)
{
    if (hardwareIdentifier.empty())
    {
        throw FirmwareError("The connected controller did not report a unique identifier, so there is nothing to "
                            "check a firmware file against.");
    }

    std::ifstream stream(path, std::ios::binary);

    if (!stream)
        throw FirmwareError("'" + path + "' could not be opened for reading.");

    const Frame container((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    if (!stream.eof() && stream.fail())
        throw FirmwareError("'" + path + "' could not be read.");

    const std::string shortName = baseName(path);

    if (container.size() < (HeaderLength + BlockLength))
    {
        throw FirmwareError("'" + shortName + "' is only " + std::to_string(container.size())
                            + " bytes long, which is too short to be a firmware file.");
    }

    // A container is always a whole number of cipher blocks. Anything else cannot have come out of the
    // packaging tool, and the boot loader refuses a piece that is not one.
    if ((container.size() % BlockLength) != 0)
    {
        throw FirmwareError("'" + shortName + "' is " + std::to_string(container.size())
                            + " bytes long, which is not a multiple of " + std::to_string(BlockLength)
                            + ". The file is truncated or is not a ScopeLink firmware file.");
    }

    // The file is encrypted against the target controller's own identifier and this driver holds no key,
    // so the name is the only thing about the intended target it can read. The boot loader would catch
    // the mistake as well - on the first piece, before it programs anything - but only after the slot
    // holding the working firmware had been erased for it, which is a poor way to find out that somebody
    // renamed a file.
    if (!nameMatchesController(path, hardwareIdentifier))
    {
        throw FirmwareError("'" + shortName + "' is not named after this ScopeLink, which is " + hardwareIdentifier
                            + ". Every firmware file is encrypted for one particular unit and is named after it, so "
                            + "a file with another name either belongs to another unit or has been renamed. Restore "
                            + "the name the file was delivered with.");
    }

    return FirmwareFile(path, container);
}

void FirmwareFile::requireFitsSlot(const BootInfo &info) const
{
    // The container is the firmware padded up to a whole flash page and carrying a one block header, so
    // it can be longer than the image and still be right; beyond that it was packaged for another part.
    const size_t largest = info.slotSize + HeaderLength;

    if (m_container.size() > largest)
    {
        throw FirmwareError("'" + name() + "' is " + std::to_string(m_container.size())
                            + " bytes and this ScopeLink can take at most " + std::to_string(largest)
                            + ". The file was packaged for a controller with a larger firmware slot, and the "
                            + "controller has not been changed.");
    }
}

// ---------------------------------------------------------------------------------------------------
// The update
// ---------------------------------------------------------------------------------------------------

FirmwareUpdate::FirmwareUpdate(ISerialTransport &transport, FirmwareFile file)
    : m_transport(transport), m_file(std::move(file))
{
    m_protocol.reset(new Protocol(m_transport));
    m_device.reset(new Device(*m_protocol));

    m_message = "Waiting for the ScopeLink to come back in its boot loader";

    beginWait(BootloaderTimeoutMs);
}

FirmwareUpdate::~FirmwareUpdate()
{
    closePort();
}

void FirmwareUpdate::setLogger(Logger logger)
{
    m_logger = logger;

    m_protocol->setLogger(logger);
    m_device->setLogger(std::move(logger));
}

int FirmwareUpdate::percent() const
{
    switch (m_stage)
    {
        case Stage::WaitingForBootloader:
            return 0;

        case Stage::Erasing:
            return 5;

        case Stage::Sending:
        {
            const size_t total = m_file.container().size();

            // Ten to ninety, so that the erase before it and the verification after it both have somewhere
            // to be: the transfer is the long part but it is not the whole update.
            return (total == 0) ? 10 : static_cast<int>(10 + ((80 * m_sent) / total));
        }

        case Stage::Verifying:
            return 90;

        case Stage::Starting:
            return 95;

        case Stage::WaitingForFirmware:
            return 97;

        default:
            return 100;
    }
}

Device &FirmwareUpdate::device()
{
    if (m_stage != Stage::Complete)
        throw FirmwareError("The firmware update has not finished, so there is no controller to talk to yet.");

    return *m_device;
}

void FirmwareUpdate::restartController()
{
    if (m_stage != Stage::Complete)
        throw FirmwareError("There is no controller to restart until the firmware update has finished.");

    // Reported by the device layer rather than thrown, and deliberately not acted on either way: a
    // controller that restarted before its answer got out looks exactly like one that never heard the
    // request, and the wait below is what tells them apart.
    m_device->requestReset();

    m_restarting = true;

    closePort();
    enter(Stage::WaitingForFirmware, "Restarting the ScopeLink so that it reads the restored configuration");
    beginWait(FirmwareTimeoutMs);
}

void FirmwareUpdate::close()
{
    closePort();
}

bool FirmwareUpdate::step()
{
    try
    {
        switch (m_stage)
        {
            case Stage::WaitingForBootloader:
                return waitForBootloader();

            case Stage::Erasing:
                return erase();

            case Stage::Sending:
                return send();

            case Stage::Verifying:
                return verify();

            case Stage::Starting:
                return start();

            case Stage::WaitingForFirmware:
                return waitForFirmware();

            default:
                return false;
        }
    }
    catch (const std::exception &error)
    {
        return fail(error.what());
    }
}

bool FirmwareUpdate::waitForBootloader()
{
    if (Clock::now() < m_nextAttempt)
        return true;

    m_nextAttempt = Clock::now() + std::chrono::milliseconds(ProbeIntervalMs);

    try
    {
        openPort();

        // Short, because a probe that finds nothing is the normal outcome for the first few seconds and
        // the protocol layer spends this three times over on every one of them.
        setTimeout(ProbeTimeoutMs);

        BootInfo info;

        if (BootInfo::read(*m_protocol, info))
        {
            // Both of these can still refuse the update for nothing: the erase below is the first thing
            // that changes the controller.
            info.requireSupported();
            m_file.requireFitsSlot(info);

            m_bootInfo = info;

            setTimeout(RequestTimeoutMs);
            report("The ScopeLink is in its boot loader (" + info.toString() + ")");

            if (info.reportsReadProtection && !info.isReadProtected)
            {
                // Worth saying rather than acting on. The boot loader arms the protection itself on its
                // first run and reports nothing when it does, so this is the only place a unit whose
                // option byte programming failed becomes visible.
                log("firmware", "This controller's flash is not read protected.");
            }

            if (info.hasStartableFirmware)
            {
                log("firmware", "Replacing the firmware now in the slot: " + std::to_string(info.storedImageSize)
                                    + " bytes, checksum " + hex32(info.storedImageChecksum));
            }
            else
            {
                log("firmware", "The firmware slot is empty or holds an unfinished download.");
            }

            enter(Stage::Erasing, "Erasing the firmware slot, " + std::to_string(info.pageCount) + " page(s) of "
                                      + std::to_string(info.pageSize) + " bytes");

            return true;
        }

        m_lastProbeFailure = "something answered on " + m_transport.name() + " but not as a boot loader";
        closePort();
    }
    catch (const FirmwareError &)
    {
        // A boot loader that answered with something this driver cannot drive, or a file it will not
        // take. Waiting will not improve either, and the message says far more than the timeout would.
        throw;
    }
    catch (const std::exception &error)
    {
        m_lastProbeFailure = error.what();

        // The port may simply not be back yet. Let it go rather than keeping a handle to a device that
        // has not finished enumerating.
        closePort();
    }

    if (waiting())
        return true;

    return fail(describeMissingBootloader());
}

std::string FirmwareUpdate::describeMissingBootloader()
{
    // Only reached once the wait has already failed, so it can afford one more transaction. The firmware
    // answers an identifier the boot loader does not, which is what tells a controller that never
    // restarted apart from one that is not there at all - two quite different things to be told.
    bool firmwareAnswering = false;

    try
    {
        openPort();
        setTimeout(ProbeTimeoutMs);

        m_protocol->transact(command::InterfaceVersion, {}, 8);
        firmwareAnswering = true;
    }
    catch (const std::exception &)
    {
    }

    closePort();

    if (firmwareAnswering)
    {
        return "The ScopeLink on " + m_transport.name()
               + " is still running its firmware and did not restart into its boot loader. Its firmware may be too "
                 "old to be updated over USB. The controller has not been changed.";
    }

    return "No ScopeLink boot loader appeared on " + m_transport.name() + " within "
           + std::to_string(BootloaderTimeoutMs / 1000) + " seconds. The controller has not been changed. Last error: "
           + (m_lastProbeFailure.empty() ? std::string("none") : m_lastProbeFailure);
}

bool FirmwareUpdate::erase()
{
    setTimeout(EraseTimeoutMs);

    Frame response;

    try
    {
        response = m_protocol->transact(boot::EraseSlot, {}, Protocol::HeaderLength + 1);
    }
    catch (...)
    {
        setTimeout(RequestTimeoutMs);
        throw;
    }

    setTimeout(RequestTimeoutMs);
    boot::check("erase the firmware slot", response);

    m_sent        = 0;
    m_sendStarted = Clock::now();

    const Frame &container = m_file.container();
    const size_t pieces    = (container.size() + m_bootInfo.chunkSize - 1) / m_bootInfo.chunkSize;

    enter(Stage::Sending, "Sending " + std::to_string(container.size()) + " bytes in " + std::to_string(pieces)
                              + " piece(s) of " + std::to_string(m_bootInfo.chunkSize));

    return true;
}

bool FirmwareUpdate::send()
{
    const Frame &container        = m_file.container();
    const Clock::time_point until = Clock::now() + std::chrono::milliseconds(SendBudgetMs);

    while (m_sent < container.size())
    {
        const size_t length = std::min(static_cast<size_t>(m_bootInfo.chunkSize), container.size() - m_sent);

        sendChunk(m_sent, length);

        m_sent += length;

        // The loop gives the caller's event loop its thread back on a time budget rather than after a
        // fixed number of pieces, so the progress the user sees advances at the same rate whatever the
        // link and the controller turn out to manage.
        if (Clock::now() >= until)
            break;
    }

    if (m_sent < container.size())
        return true;

    const double seconds = std::chrono::duration<double>(Clock::now() - m_sendStarted).count();

    log("firmware", "Sent " + std::to_string(container.size()) + " bytes in " + std::to_string(seconds).substr(0, 4)
                        + " s");

    enter(Stage::Verifying, "Checking what the controller received");

    return true;
}

void FirmwareUpdate::sendChunk(size_t offset, size_t length)
{
    Frame payload(4 + length);

    payload[0] = static_cast<uint8_t>((offset >> 24) & 0xff);
    payload[1] = static_cast<uint8_t>((offset >> 16) & 0xff);
    payload[2] = static_cast<uint8_t>((offset >> 8) & 0xff);
    payload[3] = static_cast<uint8_t>(offset & 0xff);

    std::copy(m_file.container().begin() + static_cast<long>(offset),
              m_file.container().begin() + static_cast<long>(offset + length), payload.begin() + 4);

    const Frame response = m_protocol->transact(boot::WriteData, payload, Protocol::HeaderLength + 1);
    const uint8_t result = byte_order::toByte(response, Protocol::HeaderLength);

    if (result == boot::ResultOk)
        return;

    // The boot loader carries the cipher's chaining vector from one piece to the next, so the pieces are
    // strictly ordered and it keeps track of where it has got to. That makes a piece un-resendable: if an
    // acknowledgement is lost on the way back, the protocol layer's retry arrives at an offset the boot
    // loader has already passed and is refused as out of order. Nothing here can recover it - the
    // transfer has to start again from the erase - so it is named for what it is rather than reported as
    // a controller that would not take the data.
    if (result == boot::ResultSequenceError)
    {
        throw FirmwareError("The transfer lost its place " + std::to_string(offset)
                            + " bytes in. This is almost always a dropped USB frame, and a transfer cannot be picked "
                            + "up again part way through, so the firmware has to be sent again from the beginning.");
    }

    throw FirmwareError("The controller would not take the firmware at offset " + std::to_string(offset) + ": "
                        + boot::describeResult(result) + ".");
}

bool FirmwareUpdate::verify()
{
    // The checksum is calculated over the whole slot before the answer goes out, so this one request
    // takes far longer than the hundred that preceded it.
    setTimeout(FinishTimeoutMs);

    Frame response;

    try
    {
        response = m_protocol->transact(boot::Finish, {}, Protocol::HeaderLength + 5);
    }
    catch (...)
    {
        setTimeout(RequestTimeoutMs);
        throw;
    }

    setTimeout(RequestTimeoutMs);

    const uint8_t result   = byte_order::toByte(response, Protocol::HeaderLength);
    const uint32_t checksum = byte_order::toUInt(response, Protocol::HeaderLength + 1, 4);

    if (result != boot::ResultOk)
    {
        // The checksum comes back whether the image was accepted or not, so it is quoted here as well: it
        // is the difference between a firmware that arrived damaged and one that never arrived in full.
        throw FirmwareError("The controller refused the firmware it had just been sent: "
                            + boot::describeResult(result) + " (it calculated " + hex32(checksum)
                            + " over what reached it). The firmware has not been started, and the controller is "
                            + "still in its boot loader.");
    }

    // The container carries the checksum the boot loader compared this against, encrypted, so there is
    // nothing here to compare it with a second time. It is logged because it is what identifies the build
    // that is now on the controller.
    log("firmware", "The controller verified the firmware it programmed, checksum " + hex32(checksum));

    enter(Stage::Starting, "Starting the new firmware");

    return true;
}

bool FirmwareUpdate::start()
{
    boot::check("start the new firmware", m_protocol->transact(boot::StartDrive, {}, Protocol::HeaderLength + 1));

    // The boot loader answers first and resets a few milliseconds later, so the port is let go now rather
    // than being held open onto a device that is about to leave the bus.
    closePort();

    enter(Stage::WaitingForFirmware, "Waiting for the ScopeLink to start the new firmware");
    beginWait(FirmwareTimeoutMs);

    return true;
}

bool FirmwareUpdate::waitForFirmware()
{
    if (Clock::now() < m_nextAttempt)
        return true;

    m_nextAttempt = Clock::now() + std::chrono::milliseconds(ProbeIntervalMs);

    try
    {
        openPort();
        setTimeout(Device::ReceiveTimeoutMs);

        m_device->open();

        m_stage = Stage::Complete;

        report("The ScopeLink is running " + m_device->identification().softwareIdentifier);

        return false;
    }
    catch (const std::exception &error)
    {
        m_lastProbeFailure = error.what();
        closePort();
    }

    if (waiting())
        return true;

    const std::string what = m_restarting ? "The firmware and the configuration are both written, but the ScopeLink "
                                            "did not come back after being restarted"
                                          : "The new firmware was written and verified, but the ScopeLink did not "
                                            "come back";

    return fail(what + " on " + m_transport.name() + " within " + std::to_string(FirmwareTimeoutMs / 1000)
                + " seconds. Unplug it and plug it back in. If it comes back under a different port name, point the "
                + "driver at its /dev/serial/by-id path instead, which does not change. Last error: "
                + m_lastProbeFailure);
}

// ---------------------------------------------------------------------------------------------------
// Plumbing
// ---------------------------------------------------------------------------------------------------

void FirmwareUpdate::openPort()
{
    if (m_transport.isOpen())
        return;

    // reopen() rather than an open() of its own: it is the one way of asking any transport for a fresh
    // port, and the reason a failure gives is fetched separately because reopen() can only answer no.
    if (!m_transport.reopen())
    {
        const std::string reason = m_transport.lastError();

        throw CommunicationError("Cannot open " + m_transport.name()
                                 + (reason.empty() ? std::string(".") : (": " + reason)));
    }
}

void FirmwareUpdate::closePort()
{
    m_transport.close();
}

void FirmwareUpdate::setTimeout(int timeoutMs)
{
    m_transport.setReceiveTimeout(timeoutMs);
}

bool FirmwareUpdate::waiting() const
{
    return Clock::now() < m_deadline;
}

void FirmwareUpdate::beginWait(int timeoutMs)
{
    m_deadline    = Clock::now() + std::chrono::milliseconds(timeoutMs);
    m_nextAttempt = Clock::now() + std::chrono::milliseconds(ProbeIntervalMs);
}

void FirmwareUpdate::enter(Stage stage, const std::string &message)
{
    m_stage = stage;

    report(message);
}

bool FirmwareUpdate::fail(const std::string &reason)
{
    m_stage   = Stage::Failed;
    m_failure = reason;
    m_message = reason;

    log("firmware", "The update stopped: " + reason);

    closePort();

    return false;
}

void FirmwareUpdate::report(const std::string &message)
{
    m_message = message;

    log("firmware", message);
}

void FirmwareUpdate::log(const char *scope, const std::string &message) const
{
    if (m_logger)
        m_logger(scope, message);
}

} // namespace scopelink
