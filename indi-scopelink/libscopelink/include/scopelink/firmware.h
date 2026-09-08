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

#pragma once

#include "scopelink/device.h"
#include "scopelink/protocol.h"
#include "scopelink/transport.h"
#include "scopelink/types.h"

#include <chrono>
#include <memory>
#include <string>

namespace scopelink
{

/** @brief Something went wrong with a firmware update rather than with the link underneath it. */
class FirmwareError : public std::runtime_error
{
    public:
        explicit FirmwareError(const std::string &what) : std::runtime_error(what) {}
};

/**
 * @brief The services a ScopeLink answers while it is sitting in its boot loader.
 *
 * The boot loader is the vendor's own, not the one burnt into the microcontroller, and it is a USB CDC
 * device exactly as the firmware is - same descriptors, same serial number, same wire framing. So an
 * update needs no second transport, no second driver and no DFU: the controller restarts, the same port
 * comes back, and these identifiers answer on it instead of the firmware's.
 *
 * The identifiers sit at 0x30 and above, clear of the range the firmware uses. Two are answered by both
 * images - the hardware and the software identification - which is what lets a unit be named without
 * knowing which of the two is running. The firmware has no handler for @ref GetBootInfo at all, so it is
 * the silence that says the firmware is the one answering.
 *
 * This must stay in step with <tt>_00_Bootloader/boot_diag.h</tt> and
 * <tt>_00_Bootloader/Configuration/pdu_handler_conf.h</tt> in the firmware project.
 */
namespace boot
{
constexpr uint8_t GetBootInfo = 0x30; /**< Report that the boot loader is running and describe the slot. */
constexpr uint8_t EraseSlot   = 0x31; /**< Erase the firmware slot, which is also what opens a download. */
constexpr uint8_t WriteData   = 0x32; /**< Hand over one piece of the container. */
constexpr uint8_t Finish      = 0x35; /**< Close the download and have the image verified. */
constexpr uint8_t StartDrive  = 0x36; /**< Start the firmware that has been downloaded. */

/**
 * Hand the device to the microcontroller's own DFU boot loader. Deliberately not used and not offered:
 * it is how the boot loader itself is replaced in the factory, there is no way back from it other than a
 * DFU download, and nothing a customer does needs it. It is named here so that the identifier is
 * recorded rather than looking free.
 */
constexpr uint8_t EnterDfu = 0x38;

/** Version of the boot loader interface this driver speaks, see BOOT_DIAG_PROTOCOL_VERSION. */
constexpr uint8_t ProtocolVersion = 5;

constexpr uint8_t ResultOk             = 0x01;
constexpr uint8_t ResultFlashError     = 0x02;
constexpr uint8_t ResultInvalidRequest = 0x03;
constexpr uint8_t ResultSequenceError  = 0x04;
constexpr uint8_t ResultCrcError       = 0x05;
constexpr uint8_t ResultNoImage        = 0x06;

/** @brief What a result code means, in the words an error message wants. */
std::string describeResult(uint8_t result);
} // namespace boot

/**
 * @brief What the boot loader says about itself and about the firmware slot.
 *
 * The geometry is read from the controller rather than compiled in, so this driver holds no knowledge of
 * which microcontroller a given ScopeLink is built on. A part with a different flash layout reports its
 * own and the download follows it.
 */
struct BootInfo
{
        /** Shortest payload that can still be made sense of; only the read protection came later. */
        static constexpr size_t MinimumPayloadLength = 25;

        /** Payload length of the interface version this driver was written against. */
        static constexpr size_t FullPayloadLength = 26;

        int protocolVersion{ 0 };
        uint32_t slotAddress{ 0 };
        uint32_t slotSize{ 0 };
        int pageSize{ 0 };
        int pageCount{ 0 };

        /** Largest piece of container the boot loader takes in one request. */
        int chunkSize{ 0 };

        /** True when the slot holds a firmware the boot loader is willing to start. */
        bool hasStartableFirmware{ false };

        uint32_t storedImageSize{ 0 };
        uint32_t storedImageChecksum{ 0 };

        /**
         * Whether the controller's flash is read protected. Reported, never changed: the boot loader arms
         * it on its first run, and nothing an update does needs it taken off. A controller that says it is
         * not protected is one whose option byte programming failed, which is worth knowing but is not a
         * reason to refuse it.
         */
        bool isReadProtected{ false };

        /** False when the controller is too old to report the read protection at all. */
        bool reportsReadProtection{ false };

        /**
         * @brief Asks whether a boot loader is answering, and reads what it says.
         * @param protocol Open link to the controller
         * @param info Filled in when a boot loader answered
         * @return True when the boot loader answered, false when the firmware is the one running
         * @throws FirmwareError A boot loader answered with something unusable.
         *
         * The firmware serves no handler for this identifier and therefore says nothing at all, so a
         * transaction that fails is an expected answer rather than a fault. A single zero byte - the shape
         * the boot loader uses to refuse a malformed request - is understood as the firmware too.
         */
        static bool read(Protocol &protocol, BootInfo &info);

        /**
         * @brief Checks that this boot loader is one the driver can drive, and says why if it is not.
         * @throws FirmwareError The boot loader cannot be driven by this driver.
         *
         * There is deliberately no attempt to fall back to an older interface: the identifiers moved when
         * the interface reached version 5, so an older boot loader answers a different identifier for the
         * very request that reports the version, and would be taken for the firmware rather than
         * misunderstood.
         */
        void requireSupported() const;

        /** @brief One line summary, for the log. */
        std::string toString() const;
};

/**
 * @brief An encrypted firmware file, on its way to the controller unopened.
 *
 * The file is a container the vendor's packaging tool produced:
 * <tt>AES128-CBC( header | firmware image | padding to a whole flash page )</tt>, encrypted with a key the
 * boot loader holds and an initialisation vector taken from the target controller's own unique identifier.
 *
 * This driver does not decrypt it and holds no key. That is a deliberate choice rather than an omission.
 * The boot loader checks the header on the very first piece it receives, before it programs a single byte,
 * so a container meant for another unit, of another layout version or describing an implausible image is
 * refused there with nothing written; and the checksum at the end is calculated over the flash the image
 * actually landed in, which is a stronger statement than any check made against the file here could be.
 * Decrypting the file first would move only one failure earlier - an erase that the retry has to redo in
 * any case - and it would mean carrying the container key in a public source tree.
 *
 * What is checked here is everything the shape of the file gives away for nothing: that it is a whole
 * number of cipher blocks, that it is long enough to hold a header and something after it, that it fits
 * the slot the controller reports, and that it is named after the unit it is about to be written to.
 *
 * This must stay in step with <tt>_20_BootDriveCommon/image_header.h</tt> in the firmware project.
 */
class FirmwareFile
{
    public:
        /** Length of a cipher block, which a container is always a whole number of. */
        static constexpr size_t BlockLength = 16;

        /** Length of the container header, exactly one cipher block. */
        static constexpr size_t HeaderLength = 16;

        /**
         * @brief Reads a firmware file and checks everything about it that needs no key.
         * @param path File to read
         * @param hardwareIdentifier Unique identifier of the controller it is meant for, 24 hex characters
         * @throws FirmwareError The file cannot be used with this controller.
         */
        static FirmwareFile open(const std::string &path, const std::string &hardwareIdentifier);

        /** @brief The file, exactly as it is handed to the controller. */
        const Frame &container() const { return m_container; }

        const std::string &path() const { return m_path; }

        /** @brief The file name on its own, for messages. */
        std::string name() const;

        /**
         * @brief Checks that this file fits the firmware slot the controller has reported.
         * @throws FirmwareError The container cannot be written to this controller.
         *
         * Deliberately checked before the erase rather than after it. The boot loader refuses an oversized
         * container too, but only once the slot holding the working firmware has been emptied for it.
         */
        void requireFitsSlot(const BootInfo &info) const;

        /**
         * @brief Reports whether a file's name identifies the controller it is about to be written to.
         *
         * The vendor names every container after the unit it was packaged for, and that name is the only
         * thing about the target a driver without the key can read. A mismatch is therefore taken
         * seriously rather than passed on to the controller to discover.
         */
        static bool nameMatchesController(const std::string &path, const std::string &hardwareIdentifier);

    private:
        FirmwareFile(std::string path, Frame container);

        std::string m_path;
        Frame m_container;
};

/**
 * @brief Writes a firmware file into a controller, one step per call.
 *
 * @par Why it is stepped rather than run
 * The driver has one thread, libindi's event loop, and it is the thread that publishes properties. An
 * update takes the better part of a minute, most of it spent waiting for a device to re-enumerate, and a
 * driver that ran it in one call would go silent for all of it - no progress, no log, nothing the user
 * could tell apart from a crash. So this advances by one bounded piece of work per @ref step and the
 * caller's own timer paces it. The command line tool drives the same object in a loop.
 *
 * @par Who owns the port
 * This does, for the whole update. The controller leaves the USB bus twice - once into the boot loader and
 * once back out - and each time the file descriptor the connection plugin opened is dead and a new one has
 * to be opened for the device that came back. The plugin cannot do that without going through the whole
 * connection state machine, so the caller closes its descriptor and this opens its own on the same path.
 *
 * @par What survives a failure
 * Everything up to the erase leaves the controller exactly as it was. From the erase onwards it has no
 * firmware until the download completes, and that window cannot be removed - only kept short and made
 * recoverable. It is recoverable because the boot loader lives in a part of the flash a download cannot
 * write to, and because nothing marks an image startable until the boot loader has read the slot back and
 * found the checksum the container promised. An interrupted download therefore leaves a controller that
 * refuses to start anything, still answering on its port, ready to be sent the file again.
 */
class FirmwareUpdate
{
    public:
        /** @brief Where the update has got to. */
        enum class Stage
        {
            WaitingForBootloader, /**< The controller is restarting into its boot loader. */
            Erasing,              /**< The firmware slot is being emptied. This is the point of no return. */
            Sending,              /**< The container is going over the link. */
            Verifying,            /**< The controller is checksumming what it programmed. */
            Starting,             /**< The new firmware has been asked to start. */
            WaitingForFirmware,   /**< The controller is restarting into the firmware just written. */
            Complete,             /**< The controller is running the new firmware and has been identified. */
            Failed                /**< The update stopped; see failure(). */
        };

        /** Receive timeout for an ordinary boot loader request, and for one piece of a download. */
        static constexpr int RequestTimeoutMs = 1000;

        /**
         * Receive timeout for the erase. The flash controller stalls the processor for the whole of it, so
         * the answer only comes once every page of the slot is done.
         */
        static constexpr int EraseTimeoutMs = 20000;

        /** Receive timeout for the closing checksum, which is calculated over the whole slot. */
        static constexpr int FinishTimeoutMs = 10000;

        /** How long to wait for a controller to come back in its boot loader. */
        static constexpr int BootloaderTimeoutMs = 30000;

        /**
         * How long to wait for the new firmware to come back and answer. More generous than the wait for
         * the boot loader: that one waits for code that was already there, this one for code that has just
         * arrived, on a device the kernel has to enumerate and udev has to apply permissions to.
         */
        static constexpr int FirmwareTimeoutMs = 40000;

        /**
         * @param transport Port to the controller, which the caller owns and must have closed
         * @param file Firmware file to write
         *
         * The transport has to be one that owns its port - a PosixSerialTransport rather than a borrowed
         * descriptor - because the controller leaves the USB bus twice during an update and each time the
         * descriptor that was open before it went describes a device that is no longer there. It is taken
         * by reference rather than created here so that the whole update can be run against a scripted
         * transport in the tests, which is the only way to exercise a sequence that ends by erasing the
         * firmware of whatever is on the other end.
         */
        FirmwareUpdate(ISerialTransport &transport, FirmwareFile file);
        ~FirmwareUpdate();

        void setLogger(Logger logger);

        /**
         * @brief Advances the update by one bounded piece of work.
         * @return True while the update is still running, false once it has finished either way.
         *
         * Never throws: a failure becomes @ref Stage::Failed with the reason in @ref failure, because the
         * caller is a timer callback with nowhere to report an exception to.
         */
        bool step();

        Stage stage() const { return m_stage; }

        /** @brief How far through the update is, 0 to 100. */
        int percent() const;

        /** @brief What is being done now, in the words the user is shown. */
        const std::string &message() const { return m_message; }

        /** @brief Why the update stopped, empty unless the stage is @ref Stage::Failed. */
        const std::string &failure() const { return m_failure; }

        /** @brief What the boot loader reported. Only meaningful once the erase has started. */
        const BootInfo &bootInfo() const { return m_bootInfo; }

        /**
         * @brief The controller running the new firmware.
         * @throws FirmwareError The update has not reached @ref Stage::Complete.
         *
         * Handed out so that the caller can put the configuration back over the link that has just been
         * established, rather than closing it and opening a third one to do the same thing.
         */
        Device &device();

        /**
         * @brief Restarts the controller and goes back to waiting for it, so @ref step resumes.
         * @throws FirmwareError The update has not reached @ref Stage::Complete.
         *
         * For the caller that has just had to write configuration values back. The new firmware has been
         * running since the download finished, but it started up by reading a dataset the old firmware
         * wrote: if the two lay that memory out differently, the values it took at start up were
         * misread, and putting the stored bytes right does not put the running copy right. Restarting is
         * what makes it read the restored configuration the way it will on every power up from now on.
         *
         * A caller that had nothing to write back has nothing to gain from this and should not call it.
         */
        void restartController();

        /** @brief Closes the port so that the connection plugin can have it back. */
        void close();

    private:
        using Clock = std::chrono::steady_clock;

        /** How long one call may spend handing pieces of the container over before it gives the loop back. */
        static constexpr long SendBudgetMs = 100;

        /** How long to leave between attempts to find a controller that is still re-enumerating. */
        static constexpr long ProbeIntervalMs = 500;

        /**
         * Receive timeout while hunting for a boot loader that may not be there yet. Deliberately short:
         * finding nothing is the normal outcome for the first few seconds and the protocol layer spends
         * this three times over on every attempt.
         */
        static constexpr int ProbeTimeoutMs = 300;

        bool waitForBootloader();
        bool waitForFirmware();
        bool erase();
        bool send();
        bool verify();
        bool start();

        void sendChunk(size_t offset, size_t length);

        /** @brief Works out why no boot loader turned up, and says so in the user's terms. */
        std::string describeMissingBootloader();

        void openPort();
        void closePort();
        void setTimeout(int timeoutMs);
        bool waiting() const;
        void beginWait(int timeoutMs);

        /**
         * @brief Moves to a stage and says what that stage is about to do.
         *
         * Always called at the end of the step that finished the stage before, and every caller returns
         * to its own caller immediately afterwards. That is what puts the message in front of the user
         * before the work it describes begins - which matters because two of these steps block for
         * several seconds, and a message that arrives once the step is over is exactly the message nobody
         * needed.
         */
        void enter(Stage stage, const std::string &message);

        bool fail(const std::string &reason);
        void report(const std::string &message);
        void log(const char *scope, const std::string &message) const;

        ISerialTransport &m_transport;
        FirmwareFile m_file;
        Logger m_logger;

        std::unique_ptr<Protocol> m_protocol;
        std::unique_ptr<Device> m_device;

        Stage m_stage{ Stage::WaitingForBootloader };
        std::string m_message;
        std::string m_failure;

        BootInfo m_bootInfo;

        /** How much of the container has been handed over and acknowledged. */
        size_t m_sent{ 0 };

        Clock::time_point m_deadline;
        Clock::time_point m_nextAttempt;
        Clock::time_point m_sendStarted;

        /** Kept from the wait so that a controller that never restarted can be named as such. */
        std::string m_lastProbeFailure;

        /** True while the wait is for a closing restart rather than for the firmware just written. */
        bool m_restarting{ false };
};

} // namespace scopelink
