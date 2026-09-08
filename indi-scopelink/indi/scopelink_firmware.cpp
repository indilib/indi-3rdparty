/*
    ScopeLink INDI driver - firmware update

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink_driver.h"

#include <connectionplugins/connectionserial.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

extern const char *FIRMWARE_TAB;

/** @brief Expands a leading ~ so that a path typed by a user behaves the way they expect. */
static std::string expandFirmwarePath(const std::string &path)
{
    if ((path.size() < 2) || (path[0] != '~') || (path[1] != '/'))
        return path;

    const char *home = getenv("HOME");

    return (home == nullptr) ? path : (std::string(home) + path.substr(1));
}

/**
 * @brief Where the configuration is written before an update, and the name it is written under.
 *
 * Under the directory libindi already keeps a driver's own files in, so the backup is where a user
 * looking for it would look, and named after the unit and the moment so that a second update never writes
 * over the record of the first.
 */
static std::string backupFileName(const std::string &hardwareIdentifier)
{
    const char *home = getenv("HOME");
    std::string directory = (home == nullptr) ? std::string(".") : (std::string(home) + "/.indi");

    mkdir(directory.c_str(), 0755);

    const std::time_t now = std::time(nullptr);
    std::tm local{};
    char stamp[32];

    localtime_r(&now, &local);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);

    return directory + "/scopelink-" + hardwareIdentifier + "-" + stamp + ".par";
}

// ---------------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------------

void ScopeLink::buildFirmwareProperties()
{
    FirmwareFileTP[0].fill("PATH", "File", "");
    FirmwareFileTP.fill(getDeviceName(), "FIRMWARE_FILE", "Firmware file", FIRMWARE_TAB, IP_RW, 60, IPS_IDLE);

    FirmwareActionSP[0].fill("CHECK", "Check the file", ISS_OFF);
    FirmwareActionSP[1].fill("INSTALL", "Install firmware", ISS_OFF);
    FirmwareActionSP.fill(getDeviceName(), "FIRMWARE_ACTION", "Action", FIRMWARE_TAB, IP_RW, ISR_ATMOST1, 60,
                          IPS_IDLE);

    FirmwareProgressNP[0].fill("PERCENT", "Progress (%)", "%.0f", 0, 100, 1, 0);
    FirmwareProgressNP.fill(getDeviceName(), "FIRMWARE_PROGRESS", "Update", FIRMWARE_TAB, IP_RO, 60, IPS_IDLE);

    FirmwareStatusTP[0].fill("MESSAGE", "State", "Idle");
    FirmwareStatusTP.fill(getDeviceName(), "FIRMWARE_STATUS", "Status", FIRMWARE_TAB, IP_RO, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------------------------------
// The file
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::checkFirmwareFile(std::unique_ptr<scopelink::FirmwareFile> &file)
{
    const std::string path = expandFirmwarePath(FirmwareFileTP[0].getText());

    if (path.empty())
    {
        LOG_ERROR("Set a file name in 'Firmware file' first.");
        return false;
    }

    // Whichever image is running, the unit identifier is the one thing both of them answer, and it is
    // what every firmware file is named after.
    const std::string &identifier = m_hardwareIdentifier;

    try
    {
        file.reset(new scopelink::FirmwareFile(scopelink::FirmwareFile::open(path, identifier)));
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("%s", error.what());
        return false;
    }

    LOGF_INFO("'%s' is %zu bytes and is packaged for this ScopeLink, %s.", file->name().c_str(),
              file->container().size(), identifier.c_str());

    // Everything past this point is the controller's word rather than the file's. The file is encrypted
    // against this unit's own identifier and the driver holds no key for it, so what it contains is
    // checked by the boot loader as it arrives - on the first piece, before anything is programmed - and
    // again by a checksum taken over the flash the firmware actually landed in.
    LOG_INFO("The controller checks the contents as they arrive and verifies the result against the "
             "checksum the file carries, so a file that is damaged or meant for another unit is refused "
             "before anything is started.");

    return true;
}

// ---------------------------------------------------------------------------------------------------
// Starting
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::canStartFirmwareUpdate()
{
    if (m_update)
    {
        LOG_ERROR("A firmware update is already running.");
        return false;
    }

    if (isSimulation())
    {
        // Refused rather than pretended. A simulated update that reported success would be worse than no
        // simulation at all: the one thing anybody would want to learn from it is whether their real
        // controller survives, and it is the only part a simulated port cannot answer.
        LOG_ERROR("Firmware updates are not simulated. Turn simulation off and connect to a controller.");
        return false;
    }

    if (!m_device)
    {
        LOG_ERROR("The ScopeLink is not connected.");
        return false;
    }

    if (m_bootloaderOnly)
        return true;

    if (!isReady())
    {
        LOG_ERROR("The ScopeLink has not been identified, so there is nothing to update.");
        return false;
    }

    try
    {
        const scopelink::Status &status = m_device->requireStatus();

        // A controller that loses its firmware while a motor is running stops it wherever it happens to
        // be, and the position it has stored for that motor then describes somewhere it is not. Every
        // motor, not only the ones something is assigned to: a motor that is turning is turning whether
        // or not this driver has a name for it.
        for (int index = 0; index < status.motorCount(); index++)
        {
            if (!status.motor(index).moving)
                continue;

            LOGF_ERROR("Motor %d is moving. Wait for it to stop, or stop it, before updating the firmware.", index + 1);
            return false;
        }
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller's state could not be read, so the update was not started: %s", error.what());
        return false;
    }

    return true;
}

bool ScopeLink::saveConfigurationForUpdate()
{
    m_savedConfiguration     = m_catalogue;
    m_savedConfigurationUnit = m_hardwareIdentifier;

    size_t readable = 0;

    for (scopelink::Did &identifier : m_savedConfiguration)
    {
        if (identifier.read(*m_device))
            readable++;
    }

    if (readable == 0)
    {
        LOG_ERROR("Not one configuration value could be read from the controller, so the update was not started.");
        return false;
    }

    if (readable < m_savedConfiguration.size())
    {
        LOGF_WARN("%zu of %zu configuration values could not be read and will not be restored if the update "
                  "changes them.",
                  m_savedConfiguration.size() - readable, m_savedConfiguration.size());
    }

    m_configurationBackup = backupFileName(m_hardwareIdentifier);

    try
    {
        scopelink::did_file::write(m_configurationBackup, m_device->identification().toString(),
                                   m_savedConfiguration);

        LOGF_INFO("Configuration saved to '%s' (%zu values).", m_configurationBackup.c_str(), readable);
    }
    catch (const std::exception &error)
    {
        // On disk before the controller is touched, or not at all. Everything after this point can be
        // recovered from that file with the parameter editor, and nothing before it needs to be.
        LOGF_ERROR("The configuration could not be written to '%s', so the update was not started: %s",
                   m_configurationBackup.c_str(), error.what());
        return false;
    }

    return true;
}

void ScopeLink::startFirmwareUpdate()
{
    if (!canStartFirmwareUpdate())
        return;

    std::unique_ptr<scopelink::FirmwareFile> file;

    if (!checkFirmwareFile(file))
        return;

    if (!m_bootloaderOnly && !saveConfigurationForUpdate())
        return;

    if (m_bootloaderOnly)
    {
        LOG_INFO("The ScopeLink is already in its boot loader, so it is not asked to restart and its "
                 "configuration is not read - there is no firmware there to read it from.");
    }
    else
    {
        LOG_WARN("Updating the firmware. Do not unplug the ScopeLink until this finishes.");

        if (!m_device->requestJumpToBootloader())
        {
            // Not fatal on its own: a controller that restarted before its answer got out looks exactly
            // like one that never heard the request, and only the wait that follows can tell them apart.
            LOG_WARN("The ScopeLink did not acknowledge the request to restart into its boot loader. "
                     "Waiting to see whether it restarts anyway.");
        }
    }

    const std::string port = m_serialConnection->port();

    // From here the update owns the port. The controller is about to leave the USB bus and come back, so
    // the descriptor the connection plugin opened is about to describe a device that is no longer there,
    // and the plugin cannot open a new one without going through the whole connection state machine.
    releaseDevice();
    clearPortExclusivity();
    m_serialConnection->Disconnect();

    m_configurationRestored = false;
    m_lastUpdateMessage.clear();

    m_updatePort.reset(new scopelink::PosixSerialTransport(port, scopelink::FirmwareUpdate::RequestTimeoutMs));
    m_update.reset(new scopelink::FirmwareUpdate(*m_updatePort, *file));
    m_update->setLogger([this](const char *scope, const std::string &message)
                        { LOGF_DEBUG("[%s] %s", scope, message.c_str()); });

    FirmwareActionSP.setState(IPS_BUSY);
    FirmwareActionSP.apply();

    publishFirmwareProgress();
}

// ---------------------------------------------------------------------------------------------------
// Running
// ---------------------------------------------------------------------------------------------------

void ScopeLink::publishFirmwareProgress()
{
    if (!m_update)
        return;

    const bool failed = m_update->stage() == scopelink::FirmwareUpdate::Stage::Failed;
    const bool done   = m_update->stage() == scopelink::FirmwareUpdate::Stage::Complete;

    FirmwareProgressNP[0].setValue(m_update->percent());
    FirmwareProgressNP.setState(failed ? IPS_ALERT : (done ? IPS_OK : IPS_BUSY));
    FirmwareProgressNP.apply();

    // One log line per step rather than one per tick: the transfer alone is a couple of hundred ticks and
    // the message only changes a handful of times in the whole update.
    if (m_update->message() != m_lastUpdateMessage)
    {
        m_lastUpdateMessage = m_update->message();

        FirmwareStatusTP[0].setText(m_lastUpdateMessage);
        FirmwareStatusTP.setState(failed ? IPS_ALERT : (done ? IPS_OK : IPS_BUSY));
        FirmwareStatusTP.apply();

        if (!failed)
            LOGF_INFO("%s", m_lastUpdateMessage.c_str());
    }
}

void ScopeLink::restoreConfigurationAfterUpdate()
{
    m_configurationRestored = true;

    if (m_savedConfiguration.empty())
        return;

    scopelink::Device &device = m_update->device();

    // The saved copy outlives a failed update so that a retry can still use it, which means it can also
    // outlive the controller it came from. Writing one unit's calibration into another would be a far
    // worse outcome than not restoring anything at all.
    const std::string &unit = device.identification().hardwareIdentifier;

    if (unit != m_savedConfigurationUnit)
    {
        LOGF_WARN("The saved configuration was read from ScopeLink %s and this is %s, so it has not been "
                  "written back. The file is '%s' if it is wanted.",
                  m_savedConfigurationUnit.c_str(), unit.c_str(), m_configurationBackup.c_str());
        return;
    }

    size_t written  = 0;
    size_t failed   = 0;
    size_t unneeded = 0;

    // In catalogue order, which puts the learnt positions after the motor settings they have to fit
    // inside. That is not incidental: the firmware refuses a last position above the maximum position, so
    // restoring a unit whose travel was recalibrated only works if the maximum goes back first.
    for (scopelink::Did &saved : m_savedConfiguration)
    {
        if (!saved.isAvailable())
            continue;

        scopelink::Did live = saved;

        // Read before writing. The configuration lives outside the firmware slot and is expected to have
        // come through untouched, so in the ordinary case this writes nothing at all - which is worth
        // arranging, because every write of a value that is already stored is a flash erase cycle spent
        // on nothing.
        if (live.read(device) && (live.value() == saved.value()))
        {
            unneeded++;
            continue;
        }

        if (saved.write(device))
            written++;
        else
        {
            failed++;
            LOGF_ERROR("'%s' could not be restored: %s", saved.description().c_str(), saved.lastError().c_str());
        }
    }

    if ((written == 0) && (failed == 0))
    {
        LOGF_INFO("All %zu configuration values came through the update untouched.", unneeded);
        return;
    }

    LOGF_WARN("%zu configuration value(s) had to be written back after the update, %zu could not be. The saved "
              "copy is in '%s'.",
              written, failed, m_configurationBackup.c_str());

    if (written == 0)
        return;

    // Something had to be written back, which means the new firmware read that memory differently from
    // the old one. It has been running since the download finished on whatever it made of the values it
    // found at start up, so it is restarted to read the corrected ones the way it will from now on.
    try
    {
        m_update->restartController();
    }
    catch (const std::exception &error)
    {
        LOGF_WARN("The ScopeLink could not be restarted after its configuration was put back: %s", error.what());
    }
}

void ScopeLink::tickFirmwareUpdate()
{
    const bool running = m_update->step();

    publishFirmwareProgress();

    if (running)
        return;

    if ((m_update->stage() == scopelink::FirmwareUpdate::Stage::Complete) && !m_configurationRestored)
    {
        restoreConfigurationAfterUpdate();
        publishFirmwareProgress();

        // The restore may have asked for a restart, which puts the update back to waiting for the
        // controller; the next tick carries on from there.
        if (m_update->stage() != scopelink::FirmwareUpdate::Stage::Complete)
            return;
    }

    const bool succeeded = m_update->stage() == scopelink::FirmwareUpdate::Stage::Complete;

    if (succeeded)
    {
        LOG_INFO("The firmware update finished.");
    }
    else if (m_configurationRestored)
    {
        // The firmware is written and the configuration is back; only the closing restart went wrong.
        // Calling that a failed update would send the user looking for a problem that is not there.
        LOGF_WARN("The firmware was updated, but the controller did not come back afterwards: %s",
                  m_update->failure().c_str());
    }
    else
    {
        LOGF_ERROR("%s", m_update->failure().c_str());
    }

    m_update->close();
    m_update.reset();
    m_updatePort.reset();

    FirmwareActionSP.reset();
    FirmwareActionSP.setState(succeeded ? IPS_OK : IPS_ALERT);
    FirmwareActionSP.apply();

    reopenController("The ScopeLink could not be reopened after the firmware update. Connect again when it "
                     "is ready; if it does not answer at all, it is waiting in its boot loader and "
                     "connecting will say so.");
}

// ---------------------------------------------------------------------------------------------------
// A controller with no firmware left
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::identifyBootloader()
{
    scopelink::BootInfo info;

    try
    {
        if (!scopelink::BootInfo::read(*m_protocol, info))
            return false;
    }
    catch (const std::exception &error)
    {
        LOGF_DEBUG("The boot loader enquiry failed as well: %s", error.what());
        return false;
    }

    scopelink::Identification identification;

    try
    {
        // The unit identifier and the software name are the two things both images answer, and the first
        // of them is what names the firmware file that will get this controller running again.
        identification = scopelink::Identification::readCommon(*m_protocol);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("A ScopeLink boot loader is answering on %s but would not identify itself: %s",
                   m_serialConnection->port(), error.what());
        return false;
    }

    m_bootloaderOnly    = true;
    m_hardwareIdentifier = identification.hardwareIdentifier;

    LOG_WARN("This ScopeLink is sitting in its boot loader and has no firmware to run. That is what a firmware "
             "update that was interrupted leaves behind, and it is recoverable: put the firmware file for this "
             "unit on the Firmware page and install it.");
    LOGF_INFO("Unit %s, boot loader %s, %s", identification.hardwareIdentifier.c_str(),
              identification.softwareIdentifier.c_str(), info.toString().c_str());

    if (info.hasStartableFirmware)
    {
        LOGF_WARN("The controller does hold a firmware it says it could start (%u bytes). It did not start it, "
                  "which usually means it was told to stay here; unplugging it and plugging it back in may be "
                  "enough.",
                  info.storedImageSize);
    }

    IdentificationTP[0].setText("-");
    IdentificationTP[1].setText("-");
    IdentificationTP[2].setText(identification.hardwareIdentifier);
    IdentificationTP[3].setText(identification.softwareIdentifier + " (boot loader)");
    IdentificationTP.setState(IPS_ALERT);

    // Nothing this controller can do is worth advertising to a client that schedules unattended work
    // against it, so no focuser, no dust cap and no light box are claimed until it has firmware again.
    setDriverInterface(AUX_INTERFACE);
    syncDriverInfo();

    return true;
}
