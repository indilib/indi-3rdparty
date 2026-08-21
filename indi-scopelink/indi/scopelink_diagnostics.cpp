/*
    ScopeLink INDI driver - configuration parameters, fault store and motor calibration

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink_driver.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

extern const char *PARAMETERS_TAB;

// ---------------------------------------------------------------------------------------------------
// Configuration parameters
// ---------------------------------------------------------------------------------------------------

const char *ScopeLink::parameterGroupProperty(scopelink::DidGroup group)
{
    switch (group)
    {
        case scopelink::DidGroup::FocuserMotor:
            return "PARAMS_FOCUSER_MOTOR";

        case scopelink::DidGroup::FlapMotor:
            return "PARAMS_FLAP_MOTOR";

        case scopelink::DidGroup::Fans:
            return "PARAMS_FANS";

        case scopelink::DidGroup::Temperature:
            return "PARAMS_TEMPERATURE";

        case scopelink::DidGroup::SmartSwitch:
            return "PARAMS_SMART_SWITCH";

        default:
            return "PARAMS_OTHER";
    }
}

scopelink::Did *ScopeLink::findDid(uint32_t id)
{
    for (scopelink::Did &identifier : m_catalogue)
    {
        if (identifier.id() == id)
            return &identifier;
    }

    return nullptr;
}

void ScopeLink::buildParameterGroups()
{
    m_parameterGroups.clear();

    const scopelink::DidGroup order[] = { scopelink::DidGroup::FocuserMotor, scopelink::DidGroup::FlapMotor,
                                          scopelink::DidGroup::Fans,         scopelink::DidGroup::Temperature,
                                          scopelink::DidGroup::SmartSwitch,  scopelink::DidGroup::Miscellaneous };

    for (const scopelink::DidGroup group : order)
    {
        std::vector<uint32_t> identifiers;

        for (const scopelink::Did &identifier : m_catalogue)
        {
            if (identifier.group() == group)
                identifiers.push_back(identifier.id());
        }

        if (identifiers.empty())
            continue;

        ParameterGroup published;

        published.group       = group;
        published.identifiers = identifiers;
        published.property.reset(new INDI::PropertyNumber(identifiers.size()));

        for (size_t index = 0; index < identifiers.size(); index++)
        {
            const scopelink::Did *identifier = findDid(identifiers[index]);

            // Ranges come from the storage type, which is the only limit the protocol itself defines.
            // Anything narrower is firmware behaviour the controller enforces and reports by refusing
            // the write, which is reported to the user as a failed write rather than guessed at here.
            (*published.property)[index].fill(identifier->elementName().c_str(), identifier->description().c_str(),
                                              "%.0f", identifier->minimum(), identifier->maximum(), 1,
                                              identifier->value());
        }

        published.property->fill(getDeviceName(), parameterGroupProperty(group), scopelink::didGroupName(group),
                                 PARAMETERS_TAB, IP_RW, 60, IPS_IDLE);

        m_parameterGroups.push_back(std::move(published));
    }
}

void ScopeLink::readAllParameters()
{
    // Deliberately not isReady(): this runs during the handshake, before libindi considers the device
    // connected, and the whole point of it is to have the values ready by the time the properties are
    // published.
    if (!m_device || !m_device->isIdentified())
        return;

    size_t failed = 0;

    for (scopelink::Did &identifier : m_catalogue)
    {
        if (!identifier.read(*m_device))
        {
            failed++;
            LOGF_WARN("'%s' could not be read: %s", identifier.description().c_str(), identifier.lastError().c_str());
        }
    }

    if (failed > 0)
        LOGF_WARN("%zu of %zu configuration parameters could not be read.", failed, m_catalogue.size());
    else
        LOGF_INFO("Read %zu configuration parameters from the controller.", m_catalogue.size());

    publishParameterValues();
}

void ScopeLink::publishParameterValues()
{
    for (ParameterGroup &group : m_parameterGroups)
    {
        bool complete = true;

        for (size_t index = 0; index < group.identifiers.size(); index++)
        {
            const scopelink::Did *identifier = findDid(group.identifiers[index]);

            if (identifier == nullptr)
                continue;

            (*group.property)[index].setValue(identifier->value());

            if (!identifier->isAvailable())
                complete = false;
        }

        group.property->setState(complete ? IPS_OK : IPS_ALERT);
        group.property->apply();
    }
}

bool ScopeLink::applyParameterGroup(ParameterGroup &group, double values[], char *names[], int n)
{
    size_t written = 0;
    size_t failed  = 0;

    // Only the elements the client actually sent are written. INDI sends the whole vector on a Set, so
    // without this every parameter in the group would be rewritten to the controller's EEPROM each time
    // one of them was changed - and the EEPROM wear counters on the diagnostics page exist precisely
    // because that memory has a finite number of writes in it.
    for (int index = 0; index < n; index++)
    {
        for (size_t element = 0; element < group.identifiers.size(); element++)
        {
            scopelink::Did *identifier = findDid(group.identifiers[element]);

            if ((identifier == nullptr) || (strcmp(names[index], identifier->elementName().c_str()) != 0))
                continue;

            const int requested = static_cast<int>(values[index]);

            if (requested == identifier->value())
                break;

            if (!identifier->isInRange(requested))
            {
                LOGF_ERROR("'%s' cannot hold %d; its range is %d to %d.", identifier->description().c_str(), requested,
                           identifier->minimum(), identifier->maximum());
                failed++;
                break;
            }

            const int previous = identifier->value();

            identifier->setValue(requested);

            if (identifier->write(*m_device))
            {
                written++;
                LOGF_INFO("'%s' set to %d.", identifier->description().c_str(), requested);
            }
            else
            {
                identifier->setValue(previous);
                failed++;
                LOGF_ERROR("'%s' could not be written: %s", identifier->description().c_str(),
                           identifier->lastError().c_str());
            }

            break;
        }
    }

    // Read back rather than trusting the write: the controller is the authority on what it stored, and a
    // value it silently clamped is something the user needs to see.
    for (const uint32_t id : group.identifiers)
    {
        scopelink::Did *identifier = findDid(id);

        if (identifier != nullptr)
            identifier->read(*m_device);
    }

    publishParameterValues();

    group.property->setState((failed > 0) ? IPS_ALERT : IPS_OK);
    group.property->apply();

    if ((written == 0) && (failed == 0))
        LOG_DEBUG("No configuration parameter changed.");

    // Some of these change what the driver has to know about the hardware, so the affected values are
    // picked up again rather than waiting for a reconnect.
    scopelink::Did *focuserTravel = findDid(FocuserMaximumPositionDid);

    if ((focuserTravel != nullptr) && focuserTravel->isAvailable() && (focuserTravel->value() != m_focuserTravel)
        && (focuserTravel->value() > 0))
    {
        m_focuserTravel = focuserTravel->value();

        const int clientTravel = m_focuserTravel / m_stepMultiplier;

        FocusMaxPosNP[0].setValue(clientTravel);
        FocusMaxPosNP[0].setMinMax(0, clientTravel);
        FocusAbsPosNP[0].setMinMax(0, clientTravel);
        FocusMaxPosNP.apply();
        FocusAbsPosNP.apply();

        LOGF_INFO("Focuser travel is now %d controller steps.", m_focuserTravel);
    }

    scopelink::Did *flapTravel = findDid(FlapMaximumPositionDid);

    if ((flapTravel != nullptr) && flapTravel->isAvailable() && (flapTravel->value() != m_flapTravel)
        && (flapTravel->value() > 0))
    {
        m_flapTravel = flapTravel->value();
        CapPositionNP[0].setMinMax(0, m_flapTravel);

        LOGF_INFO("Front flap travel is now %d controller steps.", m_flapTravel);
    }

    return true;
}

/** @brief Expands a leading ~ so that a path typed by a user behaves the way they expect. */
static std::string expandPath(const std::string &path)
{
    if ((path.size() < 2) || (path[0] != '~') || (path[1] != '/'))
        return path;

    const char *home = getenv("HOME");

    return (home == nullptr) ? path : (std::string(home) + path.substr(1));
}

void ScopeLink::exportParameters()
{
    const std::string path = expandPath(ParametersFileTP[0].getText());

    if (path.empty())
    {
        LOG_ERROR("Set a file name in 'Parameter file' before exporting.");
        ParametersFileTP.setState(IPS_ALERT);
        ParametersFileTP.apply();
        return;
    }

    try
    {
        scopelink::did_file::write(path, m_device->identification().toString(), m_catalogue);
        LOGF_INFO("Exported the configuration to '%s'.", path.c_str());
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The configuration could not be exported: %s", error.what());
    }
}

void ScopeLink::importParameters()
{
    const std::string path = expandPath(ParametersFileTP[0].getText());

    if (path.empty())
    {
        LOG_ERROR("Set a file name in 'Parameter file' before importing.");
        ParametersFileTP.setState(IPS_ALERT);
        ParametersFileTP.apply();
        return;
    }

    // The file is read into a copy of the catalogue first, so that a file this controller disagrees with
    // does not leave half of the live values overwritten with numbers that were never written to hardware.
    std::vector<scopelink::Did> loaded = m_catalogue;
    size_t applied                     = 0;

    try
    {
        applied = scopelink::did_file::read(path, loaded);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The configuration could not be imported: %s", error.what());
        return;
    }

    if (applied == 0)
    {
        LOGF_WARN("'%s' holds no values this controller can use.", path.c_str());
        return;
    }

    size_t written = 0;
    size_t failed  = 0;

    for (size_t index = 0; index < m_catalogue.size(); index++)
    {
        if (loaded[index].value() == m_catalogue[index].value())
            continue;

        m_catalogue[index].setValue(loaded[index].value());

        if (m_catalogue[index].write(*m_device))
        {
            written++;
        }
        else
        {
            failed++;
            LOGF_ERROR("'%s' could not be written: %s", m_catalogue[index].description().c_str(),
                       m_catalogue[index].lastError().c_str());
        }
    }

    LOGF_INFO("Imported '%s': %zu value(s) read, %zu written, %zu failed.", path.c_str(), applied, written, failed);

    readAllParameters();
}

// ---------------------------------------------------------------------------------------------------
// Fault store and EEPROM
// ---------------------------------------------------------------------------------------------------

void ScopeLink::readFaults()
{
    std::vector<scopelink::Fault> faults;

    try
    {
        faults = scopelink::Fault::readAll(*m_device);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The fault store could not be read: %s", error.what());
        FaultStoreTP.setState(IPS_ALERT);
        FaultStoreTP.apply();
        return;
    }

    if (faults.empty())
        LOG_INFO("The fault store is empty.");

    for (size_t index = 0; index < ScopeLink::MaxShownFaults; index++)
    {
        if (index < faults.size())
            FaultStoreTP[index].setText(faults[index].summary());
        else
            FaultStoreTP[index].setText("-");
    }

    bool anyActive = false;

    for (size_t index = 0; index < faults.size(); index++)
    {
        anyActive = anyActive || faults[index].isActive;

        // The freeze frame is thirty-odd numbers, which is more than a property can usefully show but
        // exactly what a support request needs. It goes to the log, where it can be copied out whole.
        LOGF_INFO("Fault %zu: %s", index + 1, faults[index].summary().c_str());
        LOGF_INFO("  Freeze frame: %s", faults[index].snapshotText().c_str());
    }

    if (faults.size() > ScopeLink::MaxShownFaults)
    {
        LOGF_WARN("%zu faults are stored but only the first %zu are shown; the whole list is in the log above.",
                  faults.size(), ScopeLink::MaxShownFaults);
    }

    FaultStoreTP.setState(anyActive ? IPS_ALERT : (faults.empty() ? IPS_OK : IPS_BUSY));
    FaultStoreTP.apply();
}

void ScopeLink::clearFaults()
{
    std::vector<scopelink::Fault> faults;

    try
    {
        faults = scopelink::Fault::readAll(*m_device);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The fault store could not be read before clearing it: %s", error.what());
        return;
    }

    size_t cleared = 0;

    for (const scopelink::Fault &fault : faults)
    {
        try
        {
            fault.clear(*m_device);
            cleared++;
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("'%s' could not be cleared: %s", fault.codeName().c_str(), error.what());
        }
    }

    // A code whose cause is still present is raised again immediately, which is the point of reading the
    // store back rather than reporting the clear as a success and leaving it at that.
    LOGF_INFO("Cleared %zu of %zu stored fault(s).", cleared, faults.size());

    readFaults();
}

void ScopeLink::readEepromStatistics()
{
    try
    {
        const scopelink::EepromStatistics statistics = scopelink::EepromStatistics::read(*m_device);

        EepromNP[0].setValue(statistics.pageEraseCounter);
        EepromNP[1].setValue(statistics.datasetCounter);
        EepromNP[2].setValue(statistics.learntDataCounter);
        EepromNP[3].setValue(statistics.faultStoreBlock1Counter);
        EepromNP[4].setValue(statistics.faultStoreBlock2Counter);
        EepromNP[5].setValue(statistics.faultStoreBlock3Counter);
        EepromNP[6].setValue(statistics.faultStoreBlock4Counter);
        EepromNP.setState(IPS_OK);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The EEPROM wear counters could not be read: %s", error.what());
        EepromNP.setState(IPS_ALERT);
    }

    EepromNP.apply();
}

void ScopeLink::publishLinkHealth()
{
    LinkHealthTP[0].setText(m_device->healthSummary());
    LinkHealthTP.setState((m_device->protocol().consecutiveFailures() > 0) ? IPS_BUSY : IPS_OK);
    LinkHealthTP.apply();
}

// ---------------------------------------------------------------------------------------------------
// Calibration
// ---------------------------------------------------------------------------------------------------

ScopeLink::MotorChannel ScopeLink::selectedChannel() const
{
    if (CalibrationMotorSP[1].getState() == ISS_ON)
        return { "Front flap", scopelink::motor::Flap, FlapSavedPositionDid, FlapMaximumPositionDid };

    return { "Focuser", scopelink::motor::Focuser, FocuserSavedPositionDid, FocuserMaximumPositionDid };
}

void ScopeLink::publishCalibration(const scopelink::Status &status)
{
    const MotorChannel channel = selectedChannel();
    const bool isFlap          = channel.motorId == scopelink::motor::Flap;

    const scopelink::Did *saved   = findDid(channel.savedPositionDid);
    const scopelink::Did *maximum = findDid(channel.maximumPositionDid);

    CalibrationStatusNP[0].setValue(isFlap ? status.motor2Position : status.motor1Position);
    CalibrationStatusNP[1].setValue((saved != nullptr) ? saved->value() : 0);
    CalibrationStatusNP[2].setValue((maximum != nullptr) ? maximum->value() : 0);
    CalibrationStatusNP[3].setValue(isFlap ? status.motor2Load : status.motor1Load);
    CalibrationStatusNP.setState((isFlap ? status.motor2Moving : status.motor1Moving) ? IPS_BUSY : IPS_OK);
    CalibrationStatusNP.apply();
}

void ScopeLink::calibrationJog(int steps)
{
    const MotorChannel channel    = selectedChannel();
    const scopelink::Did *maximum = findDid(channel.maximumPositionDid);

    if ((maximum == nullptr) || !maximum->isAvailable())
    {
        LOG_ERROR("The motor's travel has not been read, so there is nothing to clamp a jog against.");
        return;
    }

    try
    {
        const scopelink::Status &status = m_device->requireStatus();
        const long current =
            (channel.motorId == scopelink::motor::Flap) ? status.motor2Position : status.motor1Position;

        long target = current + steps;

        // Clamped rather than refused. Jogging is how the end stops are found in the first place, and a
        // request that would run past the currently believed travel is the normal way to discover that
        // the travel is wrong.
        target = std::max<long>(0, std::min<long>(target, maximum->value()));

        m_device->moveMotor(channel.motorId, static_cast<int>(target));
        m_device->refreshStatus();

        LOGF_INFO("%s jogging from %ld to %ld.", channel.name, current, target);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the jog: %s", error.what());
    }
}

void ScopeLink::calibrationMarkMinimum()
{
    const MotorChannel channel = selectedChannel();

    try
    {
        m_device->syncMotor(channel.motorId, 0);
        m_device->refreshStatus();

        LOGF_INFO("%s: the current position is now zero.", channel.name);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the sync: %s", error.what());
    }
}

void ScopeLink::calibrationMarkMaximum()
{
    const MotorChannel channel = selectedChannel();
    scopelink::Did *maximum    = findDid(channel.maximumPositionDid);

    if (maximum == nullptr)
    {
        LOG_ERROR("This controller does not hold a travel limit for the selected motor.");
        return;
    }

    try
    {
        const scopelink::Status &status = m_device->requireStatus();
        const int current = (channel.motorId == scopelink::motor::Flap) ? status.motor2Position : status.motor1Position;

        if (current <= 0)
        {
            LOG_ERROR("The motor is at zero, which cannot also be the end of its travel. "
                      "Jog away from zero first, then mark the end of travel.");
            return;
        }

        maximum->setValue(current);

        if (!maximum->write(*m_device))
        {
            LOGF_ERROR("The controller did not accept the new travel limit: %s", maximum->lastError().c_str());
            maximum->read(*m_device);
            return;
        }

        LOGF_INFO("%s: the end of travel is now %d steps.", channel.name, current);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The travel limit could not be set: %s", error.what());
        return;
    }

    publishParameterValues();
}

void ScopeLink::calibrationReset()
{
    const MotorChannel channel = selectedChannel();

    scopelink::Did *maximum = findDid(channel.maximumPositionDid);
    scopelink::Did *saved   = findDid(channel.savedPositionDid);

    if ((maximum == nullptr) || (saved == nullptr))
    {
        LOG_ERROR("This controller does not hold the calibration values for the selected motor.");
        return;
    }

    // The uncalibrated travel is the largest value the identifier can hold, which is what the Windows
    // calibrator writes too: it lets the motor be driven anywhere while the real end stops are found.
    maximum->setValue(maximum->maximum());

    if (!maximum->write(*m_device))
    {
        LOGF_ERROR("The controller did not accept the reset travel limit: %s", maximum->lastError().c_str());
        maximum->read(*m_device);
        return;
    }

    saved->setValue(maximum->value() / 2);

    if (!saved->write(*m_device))
    {
        LOGF_ERROR("The controller did not accept the reset power-up position: %s", saved->lastError().c_str());
        saved->read(*m_device);
        return;
    }

    try
    {
        m_device->syncMotor(channel.motorId, saved->value());
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the sync: %s", error.what());
        return;
    }

    LOGF_WARN("%s calibration reset. The motor now believes it is in the middle of an unbounded travel; "
              "find both end stops and mark them before using it.",
              channel.name);

    publishParameterValues();
}
