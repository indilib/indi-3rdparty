/*
    ScopeLink INDI driver - lifecycle, focuser, cover, flat panel, fans, power and telemetry

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#include "scopelink_driver.h"

#include "config.h"

#include "scopelink/simulator.h"

#include <connectionplugins/connectionserial.h>
#include <indicom.h>

#include <sys/ioctl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

/**
 * libindi instantiates the driver from this. Since libindi 1.9 the device registers itself and the
 * ISGetProperties / ISNewXxx entry points are provided by the library, so there is no boilerplate here.
 */
static std::unique_ptr<ScopeLink> scopeLinkDriver(new ScopeLink());

const char *ROTATOR_TAB     = "Rotator";
const char *TELEMETRY_TAB   = "Telemetry";
const char *FANS_TAB        = "Fans";
const char *POWER_TAB       = "Power";
const char *PARAMETERS_TAB  = "Parameters";
const char *DIAGNOSTICS_TAB = "Diagnostics";
const char *CALIBRATION_TAB = "Calibration";
const char *FIRMWARE_TAB    = "Firmware";

namespace
{

/**
 * How long a restarted controller is given to come back before the driver gives up on it.
 *
 * Shorter than the wait after a firmware update, which has a boot loader and a firmware start in front of
 * it; what this waits out is a reset and a USB enumeration. Generous all the same, because the cost of
 * being too patient is a few more seconds of a progress light and the cost of being too impatient is a
 * user told to unplug a controller that was about to answer.
 */
constexpr int RestartTimeoutMs = 30000;

/** How often the port is tried while waiting for it. */
constexpr int RestartProbeIntervalMs = 500;

} // namespace

ScopeLink::ScopeLink() : INDI::FocuserInterface(this), INDI::RotatorInterface(this)
{
    setVersion(SCOPELINK_VERSION_MAJOR, SCOPELINK_VERSION_MINOR);
}

const char *ScopeLink::getDefaultName()
{
    return "ScopeLink";
}

bool ScopeLink::isReady() const
{
    return isConnected() && m_device && m_device->isIdentified();
}

bool ScopeLink::commandsFunctions() const
{
    return m_device && m_device->isIdentified() && m_device->capabilities().hasConfigurableMotorRoles;
}

// ---------------------------------------------------------------------------------------------------
// Property construction
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Three of the four bits are settled at connect time, because which devices this controller offers is
    // read out of it rather than known in advance: a motor has to be assigned to the focuser for there to
    // be one, and to the rotator, and the front flap needs a calibrated travel before it is a dust cap.
    // The light box is the one that is always there.
    setDriverInterface(FOCUSER_INTERFACE | LIGHTBOX_INTERFACE | AUX_INTERFACE);

    FI::initProperties(FOCUS_TAB);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);

    RI::initProperties(ROTATOR_TAB);

    // No homing: the mechanism has no reference switch, so the only thing a home could mean is step zero,
    // and driving to step zero is a move like any other. Offering it would promise a re-reference this
    // hardware cannot perform.
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);

    addAuxControls();
    setDefaultPollingPeriod(500);

    // --- Options, which have to exist before the device is connected -------------------------------

    StepMultiplierNP[0].fill("MULTIPLIER", "Controller steps per client step", "%.0f", 1, 1000, 1, 1);
    StepMultiplierNP.fill(getDeviceName(), "FOCUS_STEP_MULTIPLIER", "Step multiplier", OPTIONS_TAB, IP_RW, 60,
                          IPS_IDLE);

    DeveloperSP[0].fill("ENABLED", "Shown", ISS_OFF);
    DeveloperSP[1].fill("DISABLED", "Hidden", ISS_ON);
    DeveloperSP.fill(getDeviceName(), "DEVELOPER_OPTIONS", "Controller diagnostics", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                     60, IPS_IDLE);

    ParametersFileTP[0].fill("PATH", "File", "");
    ParametersFileTP.fill(getDeviceName(), "PARAMETERS_FILE", "Parameter file", PARAMETERS_TAB, IP_RW, 60, IPS_IDLE);

    // Where a sync left the sky relative to the mechanism. It belongs with the settings that have to exist
    // before the device connects, rather than with the hardware properties: it is restored from the
    // configuration file and then used while the rotator is being set up, and a property that only appears
    // after that is one there is nothing to restore into.
    RotatorOffsetNP[0].fill("OFFSET", "Sky - mechanism (degrees)", "%.2f", 0, 360, 0.01, 0);
    RotatorOffsetNP.fill(getDeviceName(), "ROTATOR_SYNC_OFFSET", "Sync offset", ROTATOR_TAB, IP_RO, 60, IPS_IDLE);

    // Which controller the simulated one pretends to be. Generations differ in what they offer, not only
    // in what they report, so this is the only way to see the USB hub and the smart switch diagnostics
    // appear and disappear without owning one controller of each generation.
    // Four entries for three generations, because a generation is not a frame layout. The same generation
    // 3 board runs interface 1.0 and 1.1, and 1.1 is what changes the frames - four bytes of digital
    // inputs out of the middle of both of them and the flap state onto the end of the status frame.
    // Generation 4 exists only on 1.1 and adds a third motor and four more USB ports on top of it, so it
    // needs no second entry of its own.
    SimulatedGenerationSP[0].fill("GENERATION_2", "2", ISS_OFF);
    SimulatedGenerationSP[1].fill("GENERATION_3", "3", ISS_ON);
    SimulatedGenerationSP[2].fill("GENERATION_3_IF11", "3, interface 1.1", ISS_OFF);
    SimulatedGenerationSP[3].fill("GENERATION_4", "4", ISS_OFF);
    SimulatedGenerationSP.fill(getDeviceName(), "SIMULATED_GENERATION", "Simulated hardware", OPTIONS_TAB, IP_RW,
                               ISR_1OFMANY, 60, IPS_IDLE);

    defineProperty(StepMultiplierNP);
    defineProperty(DeveloperSP);
    defineProperty(SimulatedGenerationSP);
    defineProperty(ParametersFileTP);
    defineProperty(RotatorOffsetNP);

    loadConfig(true, StepMultiplierNP.getName());
    loadConfig(true, DeveloperSP.getName());
    loadConfig(true, SimulatedGenerationSP.getName());
    loadConfig(true, ParametersFileTP.getName());
    loadConfig(true, RotatorOffsetNP.getName());

    // The sync offset has to be defined for its saved value to be read back into it, and read back before
    // the first connection, because identify() takes the offset from it. It is withdrawn again straight
    // away: a controller with no motor assigned to the rotator must not carry a rotator tab, and until one
    // has connected there is nothing that says whether it does. updateProperties() puts it back when the
    // assignment turns out to name a rotator.
    deleteProperty(RotatorOffsetNP);

    // --- Identification ------------------------------------------------------------------------------

    IdentificationTP[0].fill("HARDWARE_VERSION", "Hardware", "");
    IdentificationTP[1].fill("INTERFACE_VERSION", "Interface", "");
    IdentificationTP[2].fill("HARDWARE_ID", "Unit", "");
    IdentificationTP[3].fill("FIRMWARE", "Firmware", "");
    IdentificationTP.fill(getDeviceName(), "DEVICE_IDENTIFICATION", "Controller", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // --- Focuser temperature -------------------------------------------------------------------------

    FocusTemperatureNP[0].fill("TEMPERATURE", "Ambient (C)", "%.1f", -100, 100, 0.1, 0);
    FocusTemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // --- Cover ----------------------------------------------------------------------------------------

    CapParkSP[0].fill("PARK", "Close", ISS_OFF);
    CapParkSP[1].fill("UNPARK", "Open", ISS_OFF);
    CapParkSP.fill(getDeviceName(), "CAP_PARK", "Front flap", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    CapPositionNP[0].fill("POSITION_STEPS", "Position (steps)", "%.0f", 0, 1000000, 1, 0);
    CapPositionNP[1].fill("POSITION_RATIO", "Travel (0-1)", "%.2f", 0, 1, 0.01, 0);
    CapPositionNP.fill(getDeviceName(), "FLAP_POSITION", "Flap travel", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    FlapStateTP[0].fill("STATE", "Reported by the controller", "");
    FlapStateTP.fill(getDeviceName(), "FLAP_STATE", "Flap state", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // --- Flat panel -------------------------------------------------------------------------------------

    LightSP[0].fill("FLAT_LIGHT_ON", "On", ISS_OFF);
    LightSP[1].fill("FLAT_LIGHT_OFF", "Off", ISS_ON);
    LightSP.fill(getDeviceName(), "FLAT_LIGHT_CONTROL", "Flat panel", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                 IPS_IDLE);

    LightIntensityNP[0].fill("FLAT_LIGHT_INTENSITY_VALUE", "Duty cycle (%)", "%.0f", 0, 100, 1, 0);
    LightIntensityNP.fill(getDeviceName(), "FLAT_LIGHT_INTENSITY", "Brightness", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // --- Telemetry ---------------------------------------------------------------------------------------

    RailsNP[0].fill("SUPPLY", "Supply (V)", "%.1f", 0, 30, 0.1, 0);
    RailsNP[1].fill("FAN_A", "Rear fan (V)", "%.1f", 0, 30, 0.1, 0);
    RailsNP[2].fill("FAN_B", "Side fan (V)", "%.1f", 0, 30, 0.1, 0);
    RailsNP.fill(getDeviceName(), "POWER_RAILS", "Rails", TELEMETRY_TAB, IP_RO, 60, IPS_IDLE);

    TemperatureNP[0].fill("AMBIENT", "Ambient (C)", "%.1f", -100, 100, 0.1, 0);
    TemperatureNP[1].fill("MIRROR", "Mirror (C)", "%.1f", -100, 100, 0.1, 0);
    TemperatureNP[2].fill("DELTA_T", "Mirror - ambient (K)", "%.1f", -200, 200, 0.1, 0);
    TemperatureNP.fill(getDeviceName(), "TEMPERATURE_READINGS", "Temperatures", TELEMETRY_TAB, IP_RO, 60, IPS_IDLE);

    // The widgets are filled in at connect time by sizeHardwareProperties, because how many there are is
    // the controller's answer rather than this driver's. Only the group itself is named here.
    MotorLoadNP.fill(getDeviceName(), "MOTOR_LOAD", "Motor load", TELEMETRY_TAB, IP_RO, 60, IPS_IDLE);

    ControllerNP[0].fill("SENSOR_SUPPLY", "IR sensor supply (V)", "%.1f", 0, 30, 0.1, 0);
    ControllerNP[1].fill("CONTROLLER_SUPPLY", "Controller supply (V)", "%.1f", 0, 30, 0.1, 0);
    ControllerNP[2].fill("CONTROLLER_TEMPERATURE", "Die temperature (C)", "%.0f", -50, 150, 1, 0);
    ControllerNP[3].fill("CPU_LOAD", "CPU load (%)", "%.0f", 0, 100, 1, 0);
    ControllerNP[4].fill("PEAK_CPU_LOAD", "Peak CPU load (%)", "%.0f", 0, 100, 1, 0);
    ControllerNP[5].fill("STACK_USAGE", "Stack usage (%)", "%.0f", 0, 100, 1, 0);
    ControllerNP[6].fill("I2C_ERRORS", "I2C errors", "%.0f", 0, 65535, 1, 0);
    ControllerNP.fill(getDeviceName(), "CONTROLLER_DIAGNOSTICS", "Controller", TELEMETRY_TAB, IP_RO, 60, IPS_IDLE);

    // --- Fans ----------------------------------------------------------------------------------------------

    FanOverrideSP[0].fill("REAR", "Rear fan", ISS_OFF);
    FanOverrideSP[1].fill("SIDE", "Side fan", ISS_OFF);
    FanOverrideSP.fill(getDeviceName(), "FAN_OVERRIDE", "Manual control", FANS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    FanStateSP[0].fill("REAR", "Rear fan", ISS_OFF);
    FanStateSP[1].fill("SIDE", "Side fan", ISS_OFF);
    FanStateSP.fill(getDeviceName(), "FAN_STATE", "Commanded state", FANS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    FanTargetNP[0].fill("REAR", "Rear target dT (K)", "%.1f", 0, 5, 0.1, 0);
    FanTargetNP[1].fill("SIDE", "Side target dT (K)", "%.1f", 0, 5, 0.1, 0);
    FanTargetNP.fill(getDeviceName(), "FAN_TARGET_DT", "Automatic targets", FANS_TAB, IP_RW, 60, IPS_IDLE);

    // --- Power ----------------------------------------------------------------------------------------------

    AuxPowerSP[0].fill("AUX_1", "Output 1", ISS_OFF);
    AuxPowerSP[1].fill("AUX_2", "Output 2", ISS_OFF);
    AuxPowerSP.fill(getDeviceName(), "AUX_POWER", "Auxiliary outputs", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // As with the motor load, the lamps are filled in at connect time: a generation 3 hub has two ports
    // and reports a fault for each, and a generation 4 hub has six and reports none.
    UsbHubLP.fill(getDeviceName(), "USB_HUB_STATUS", "USB hub", POWER_TAB, IPS_IDLE);

    // --- Parameters, diagnostics and calibration ---------------------------------------------------------

    ParametersActionSP[0].fill("RELOAD", "Reload from controller", ISS_OFF);
    ParametersActionSP[1].fill("EXPORT", "Export to file", ISS_OFF);
    ParametersActionSP[2].fill("IMPORT", "Import from file", ISS_OFF);
    ParametersActionSP.fill(getDeviceName(), "PARAMETERS_ACTION", "Parameters", PARAMETERS_TAB, IP_RW, ISR_ATMOST1, 60,
                            IPS_IDLE);

    // On this tab rather than with the other controller-wide actions on the diagnostics one, because the
    // reason to press it is on this tab: the motor assignment is the one setting a controller does not act
    // on until it has been restarted.
    RestartSP[0].fill("RESTART", "Restart controller", ISS_OFF);
    RestartSP.fill(getDeviceName(), "CONTROLLER_RESTART", "Controller", PARAMETERS_TAB, IP_RW, ISR_ATMOST1, 60,
                   IPS_IDLE);

    FaultCountNP[0].fill("STORED", "Stored", "%.0f", 0, 255, 1, 0);
    FaultCountNP[1].fill("ACTIVE", "Active now", "%.0f", 0, 255, 1, 0);
    FaultCountNP.fill(getDeviceName(), "FAULT_COUNT", "Faults", DIAGNOSTICS_TAB, IP_RO, 60, IPS_IDLE);

    DiagnosticActionSP[0].fill("READ_FAULTS", "Read fault store", ISS_OFF);
    DiagnosticActionSP[1].fill("CLEAR_FAULTS", "Clear fault store", ISS_OFF);
    DiagnosticActionSP[2].fill("READ_EEPROM", "Read EEPROM counters", ISS_OFF);
    DiagnosticActionSP.fill(getDeviceName(), "DIAGNOSTIC_ACTION", "Actions", DIAGNOSTICS_TAB, IP_RW, ISR_ATMOST1, 60,
                            IPS_IDLE);

    for (size_t index = 0; index < MaxShownFaults; index++)
    {
        const std::string name  = "FAULT_" + std::to_string(index);
        const std::string label = "Fault " + std::to_string(index + 1);

        FaultStoreTP[index].fill(name.c_str(), label.c_str(), "");
    }

    FaultStoreTP.fill(getDeviceName(), "FAULT_STORE", "Fault store", DIAGNOSTICS_TAB, IP_RO, 60, IPS_IDLE);

    EepromNP[0].fill("PAGE_ERASE", "Page erases", "%.0f", 0, 1e9, 1, 0);
    EepromNP[1].fill("DATASET", "Dataset writes", "%.0f", 0, 65535, 1, 0);
    EepromNP[2].fill("LEARNT_DATA", "Learnt data writes", "%.0f", 0, 65535, 1, 0);
    EepromNP[3].fill("FAULT_BLOCK_1", "Fault block 1", "%.0f", 0, 65535, 1, 0);
    EepromNP[4].fill("FAULT_BLOCK_2", "Fault block 2", "%.0f", 0, 65535, 1, 0);
    EepromNP[5].fill("FAULT_BLOCK_3", "Fault block 3", "%.0f", 0, 65535, 1, 0);
    EepromNP[6].fill("FAULT_BLOCK_4", "Fault block 4", "%.0f", 0, 65535, 1, 0);
    EepromNP.fill(getDeviceName(), "EEPROM_STATISTICS", "EEPROM wear", DIAGNOSTICS_TAB, IP_RO, 60, IPS_IDLE);

    LinkHealthTP[0].fill("SUMMARY", "Link", "");
    LinkHealthTP.fill(getDeviceName(), "LINK_HEALTH", "Link health", DIAGNOSTICS_TAB, IP_RO, 60, IPS_IDLE);

    CalibrationMotorSP[0].fill("FOCUSER", "Focuser", ISS_ON);
    CalibrationMotorSP[1].fill("FLAP", "Front flap", ISS_OFF);
    CalibrationMotorSP.fill(getDeviceName(), "CALIBRATION_MOTOR", "Motor", CALIBRATION_TAB, IP_RW, ISR_1OFMANY, 60,
                            IPS_IDLE);

    CalibrationStatusNP[0].fill("CURRENT", "Current position", "%.0f", 0, 2147483647, 1, 0);
    CalibrationStatusNP[1].fill("SAVED", "Power-up position", "%.0f", 0, 2147483647, 1, 0);
    CalibrationStatusNP[2].fill("MAXIMUM", "End of travel", "%.0f", 0, 2147483647, 1, 0);
    CalibrationStatusNP[3].fill("LOAD", "Motor load (%)", "%.0f", 0, 100, 1, 0);
    CalibrationStatusNP.fill(getDeviceName(), "CALIBRATION_STATUS", "Travel", CALIBRATION_TAB, IP_RO, 60, IPS_IDLE);

    CalibrationStepNP[0].fill("STEPS", "Jog size (steps)", "%.0f", 1, 100000, 1, 100);
    CalibrationStepNP.fill(getDeviceName(), "CALIBRATION_STEP", "Jog", CALIBRATION_TAB, IP_RW, 60, IPS_IDLE);

    CalibrationActionSP[0].fill("JOG_IN", "Jog towards zero", ISS_OFF);
    CalibrationActionSP[1].fill("JOG_OUT", "Jog away from zero", ISS_OFF);
    CalibrationActionSP[2].fill("STOP", "Stop", ISS_OFF);
    CalibrationActionSP[3].fill("MARK_MIN", "Mark as zero", ISS_OFF);
    CalibrationActionSP[4].fill("MARK_MAX", "Mark as end of travel", ISS_OFF);
    CalibrationActionSP[5].fill("RESET", "Reset calibration", ISS_OFF);
    CalibrationActionSP.fill(getDeviceName(), "CALIBRATION_ACTION", "Action", CALIBRATION_TAB, IP_RW, ISR_ATMOST1, 60,
                             IPS_IDLE);

    buildFirmwareProperties();

    // --- Connection ------------------------------------------------------------------------------------------

    m_serialConnection = new Connection::Serial(this);

    // The controller is a CDC-ACM device, so the line rate is ignored on the wire. It is still offered
    // because libindi's serial plugin expects one, and because a user with a USB to serial adapter in
    // between needs to be able to set it.
    m_serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    m_serialConnection->setDefaultPort("/dev/ttyACM0");
    m_serialConnection->registerHandshake([this]() { return handshake(); });

    registerConnection(m_serialConnection);

    return true;
}

bool ScopeLink::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    // Which of the two motion interfaces is published is the assignment's answer, so it is settled here
    // and remembered: on the way out the assignment has already gone with the link, and the interface that
    // defined a property is the one that has to delete it again. A controller sitting in its boot loader
    // publishes neither - there is no firmware behind it to answer a single one of their commands.
    if (isConnected())
    {
        m_focuserPublished = !m_bootloaderOnly && m_roles.hasFocuser();
        m_rotatorPublished = !m_bootloaderOnly && m_roles.hasRotator();
    }

    if (m_focuserPublished)
        FI::updateProperties();

    if (m_rotatorPublished)
        RI::updateProperties();

    if (!isConnected())
    {
        m_focuserPublished = false;
        m_rotatorPublished = false;
    }

    if (isConnected())
    {
        if (m_rotatorPublished)
        {
            defineProperty(RotatorOffsetNP);

            // The direction is a setting rather than a reading, so it is restored once the interface has
            // defined the property it lives in. Element 0 of the pair is INDI_ENABLED, which is libindi's
            // convention for a switch that turns one thing on and off.
            loadConfig(true, ReverseRotatorSP.getName());

            m_rotatorReverse = ReverseRotatorSP[0].getState() == ISS_ON;
        }

        defineProperty(IdentificationTP);
        defineHardwareProperties();

        if (!m_bootloaderOnly)
        {
            if (DeveloperSP[0].getState() == ISS_ON)
                defineDeveloperProperties();

            // The travel just read from the controller is written back to the configuration file, so that
            // a client restoring an older one - after a recalibration, or after a session in simulation -
            // is not refused on the next start. It belongs here rather than beside the read because
            // saveConfig only has a property to write once the focuser interface has defined it.
            if (m_focuserPublished)
                saveConfig(true, FocusMaxPosNP.getName());
        }

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(IdentificationTP.getName());
        deleteProperty(RotatorOffsetNP);
        deleteHardwareProperties();
        deleteDeveloperProperties();
    }

    return true;
}

void ScopeLink::sizeHardwareProperties()
{
    const scopelink::Capabilities &capabilities = m_device->capabilities();

    // Numbered rather than named after a job. Up to generation 3 these read "Focuser" and "Flap", which
    // was true while the motors were wired to those jobs. The load is the motor's whatever it is driving,
    // and the status frame reports it by motor, so numbering them is what the wire actually says - the
    // Calibration tab is where the assignment turns a motor into a job it can be named after.
    MotorLoadNP.resize(static_cast<size_t>(capabilities.motorCount));

    for (int index = 0; index < capabilities.motorCount; index++)
    {
        const std::string name  = "MOTOR_" + std::to_string(index + 1);
        const std::string label = "Motor " + std::to_string(index + 1) + " (%)";

        MotorLoadNP[index].fill(name.c_str(), label.c_str(), "%.0f", 0, 100, 1, 0);
    }

    buildCalibrationChannels();

    // One lamp per port, and a fault lamp per port only on a controller that reports one. Generation 4
    // gained four ports in the space generation 3 spent on the two fault flags, and nothing is lost with
    // them: that firmware derived each fault flag from the active flag beside it.
    const int ports  = capabilities.usbDownstreamPortCount;
    const int faults = capabilities.hasUsbPowerFailureReporting ? 2 : 0;

    UsbHubLP.resize(static_cast<size_t>(ports + faults));

    for (int port = 0; port < ports; port++)
    {
        const std::string name  = "DS" + std::to_string(port + 1) + "_POWER";
        const std::string label = "Port " + std::to_string(port + 1) + " powered";

        UsbHubLP[port].fill(name.c_str(), label.c_str(), IPS_IDLE);
    }

    for (int index = 0; index < faults; index++)
    {
        const std::string name  = "DS" + std::to_string(index + 1) + "_FAULT";
        const std::string label = "Port " + std::to_string(index + 1) + " fault";

        UsbHubLP[ports + index].fill(name.c_str(), label.c_str(), IPS_IDLE);
    }
}

void ScopeLink::defineHardwareProperties()
{
    // The firmware page is the one thing a controller with no firmware can still be given, so it is
    // published first and unconditionally. Everything below it describes hardware that is only reachable
    // through a running firmware.
    defineProperty(FirmwareFileTP);
    defineProperty(FirmwareActionSP);
    defineProperty(FirmwareProgressNP);
    defineProperty(FirmwareStatusTP);

    if (m_bootloaderOnly)
        return;

    sizeHardwareProperties();

    const scopelink::Capabilities &capabilities = m_device->capabilities();

    if (capabilities.hasTemperatureSensor)
    {
        defineProperty(FocusTemperatureNP);
        defineProperty(TemperatureNP);
        defineProperty(FanTargetNP);
    }

    if (hasUsableFlap())
    {
        defineProperty(CapParkSP);
        defineProperty(CapPositionNP);
    }

    // Published wherever the controller sends one, flap or no flap: "this unit has no front flap" is a
    // thing worth being able to read, and it is the answer that says the assignment names no motor for it.
    if (capabilities.hasConfigurableMotorRoles)
        defineProperty(FlapStateTP);

    defineProperty(LightSP);
    defineProperty(LightIntensityNP);
    defineProperty(RailsNP);
    defineProperty(MotorLoadNP);
    defineProperty(FanOverrideSP);
    defineProperty(FanStateSP);

    defineProperty(AuxPowerSP);

    if (capabilities.hasUsbHub)
        defineProperty(UsbHubLP);

    // Each group's numbers first, then its settings, so that the tab reads in the firmware's own order
    // rather than putting every switch in the driver at the bottom.
    for (ParameterGroup &group : m_parameterGroups)
    {
        if (group.property != nullptr)
            defineProperty(*group.property);

        for (ParameterSwitch &entry : group.switches)
            defineProperty(*entry.property);
    }

    defineProperty(ParametersActionSP);
    defineProperty(RestartSP);

    defineProperty(FaultCountNP);
    defineProperty(DiagnosticActionSP);
    defineProperty(FaultStoreTP);
    defineProperty(EepromNP);
    defineProperty(LinkHealthTP);

    defineProperty(CalibrationMotorSP);
    defineProperty(CalibrationStatusNP);
    defineProperty(CalibrationStepNP);
    defineProperty(CalibrationActionSP);
}

void ScopeLink::deleteHardwareProperties()
{
    deleteProperty(FirmwareFileTP.getName());
    deleteProperty(FirmwareActionSP.getName());
    deleteProperty(FirmwareProgressNP.getName());
    deleteProperty(FirmwareStatusTP.getName());

    deleteProperty(FocusTemperatureNP.getName());
    deleteProperty(TemperatureNP.getName());
    deleteProperty(FanTargetNP.getName());
    deleteProperty(CapParkSP.getName());
    deleteProperty(CapPositionNP.getName());
    deleteProperty(FlapStateTP.getName());
    deleteProperty(LightSP.getName());
    deleteProperty(LightIntensityNP.getName());
    deleteProperty(RailsNP.getName());
    deleteProperty(MotorLoadNP.getName());
    deleteProperty(FanOverrideSP.getName());
    deleteProperty(FanStateSP.getName());
    deleteProperty(AuxPowerSP.getName());
    deleteProperty(UsbHubLP.getName());

    for (ParameterGroup &group : m_parameterGroups)
    {
        if (group.property != nullptr)
            deleteProperty(group.property->getName());

        for (ParameterSwitch &entry : group.switches)
            deleteProperty(entry.property->getName());
    }

    m_parameterGroups.clear();

    deleteProperty(ParametersActionSP.getName());
    deleteProperty(RestartSP.getName());

    deleteProperty(FaultCountNP.getName());
    deleteProperty(DiagnosticActionSP.getName());
    deleteProperty(FaultStoreTP.getName());
    deleteProperty(EepromNP.getName());
    deleteProperty(LinkHealthTP.getName());

    deleteProperty(CalibrationMotorSP.getName());
    deleteProperty(CalibrationStatusNP.getName());
    deleteProperty(CalibrationStepNP.getName());
    deleteProperty(CalibrationActionSP.getName());
}

void ScopeLink::defineDeveloperProperties()
{
    if (isConnected())
        defineProperty(ControllerNP);
}

void ScopeLink::deleteDeveloperProperties()
{
    deleteProperty(ControllerNP.getName());
}

// ---------------------------------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::handshake()
{
    if (identify())
        return true;

    // The connection plugin closes the port on a failed handshake, so nothing built on top of that
    // descriptor may outlive this call.
    releaseDevice();

    return false;
}

void ScopeLink::releaseDevice()
{
    m_device.reset();
    m_protocol.reset();
    m_transport.reset();

    m_catalogue.clear();
    m_calibrationChannels.clear();

    m_roles          = scopelink::MotorRoles();
    m_capTarget      = CapTarget::None;
    m_bootloaderOnly = false;
}

bool ScopeLink::identify()
{
    m_bootloaderOnly = false;
    m_stepMultiplier = static_cast<int>(StepMultiplierNP[0].getValue());

    if (m_stepMultiplier < 1)
    {
        LOGF_ERROR("The step multiplier (%d) is invalid, it must be 1 or greater.", m_stepMultiplier);
        return false;
    }

    if (isSimulation())
    {
        // The connection plugin honours simulation itself: it opens no port and leaves the descriptor at
        // -1, then runs this handshake as usual. So the only thing that changes here is what the protocol
        // layer talks to, and every path above it - identification, capabilities, parameters, faults,
        // motion - is the one a real controller takes.
        const int selected       = SimulatedGenerationSP.findOnSwitchIndex();
        const int generation     = (selected == 0) ? 2 : ((selected == 3) ? 4 : 3);
        const int interfaceMinor = (selected >= 2) ? 1 : 0;

        LOGF_INFO("Simulation is on. Answering as a generation %d controller on interface 1.%d; no port is "
                  "opened.",
                  generation, interfaceMinor);

        m_transport =
            std::unique_ptr<scopelink::ISerialTransport>(new scopelink::SimulatedTransport(generation, interfaceMinor));
    }
    else
    {
        m_transport = std::unique_ptr<scopelink::ISerialTransport>(new scopelink::FdSerialTransport(
            m_serialConnection->getPortFD(), scopelink::Device::ReceiveTimeoutMs, m_serialConnection->port()));
    }

    m_protocol = std::unique_ptr<scopelink::Protocol>(new scopelink::Protocol(*m_transport));
    m_device   = std::unique_ptr<scopelink::Device>(new scopelink::Device(*m_protocol));

    const auto sink = [this](const char *scope, const std::string &message)
    { LOGF_DEBUG("[%s] %s", scope, message.c_str()); };

    m_protocol->setLogger(sink);
    m_device->setLogger(sink);

    try
    {
        m_device->open();
    }
    catch (const scopelink::UnsupportedDeviceError &error)
    {
        LOGF_ERROR("%s", error.what());
        return false;
    }
    catch (const std::exception &error)
    {
        // One more question before the connection is refused. A controller whose firmware update was
        // interrupted answers none of the identifiers above, because what is running on it is its boot
        // loader - and that is a state this driver can do something about, so it is worth telling apart
        // from a port with nothing on it.
        if (identifyBootloader())
            return true;

        LOGF_ERROR("No ScopeLink answered on %s: %s", m_serialConnection->port(), error.what());
        return false;
    }

    const scopelink::Identification &identification = m_device->identification();
    const scopelink::Capabilities &capabilities     = m_device->capabilities();

    m_hardwareIdentifier = identification.hardwareIdentifier;

    LOGF_INFO("Connected to ScopeLink %s", identification.toString().c_str());

    IdentificationTP[0].setText(std::to_string(identification.hardwareMajor) + "."
                                + std::to_string(identification.hardwareMinor));
    IdentificationTP[1].setText(std::to_string(identification.interfaceMajor) + "."
                                + std::to_string(identification.interfaceMinor));
    IdentificationTP[2].setText(identification.hardwareIdentifier);
    IdentificationTP[3].setText(identification.softwareIdentifier);
    IdentificationTP.setState(IPS_OK);

    m_roles             = m_device->roles();
    m_rotatorSyncOffset = RotatorOffsetNP[0].getValue();

    LOGF_INFO("Motor assignment: %s.", m_roles.toString().c_str());

    if (!adoptMotorRoles())
        return false;

    // Claimed only once each role has something working behind it: Ekos reads this bitmask to decide which
    // jobs it may give the device, so a role listed here is a role it will drive unattended.
    uint16_t interfaces = LIGHTBOX_INTERFACE | AUX_INTERFACE;

    if (m_roles.hasFocuser())
        interfaces |= FOCUSER_INTERFACE;

    if (m_roles.hasRotator())
        interfaces |= ROTATOR_INTERFACE;

    if (hasUsableFlap())
        interfaces |= DUSTCAP_INTERFACE;

    setDriverInterface(interfaces);
    syncDriverInfo();

    if (!capabilities.hasTemperatureSensor)
        LOG_INFO("This unit has no temperature sensor fitted, so no temperature readings are offered.");

    m_catalogue = scopelink::buildDidCatalogue(capabilities);

    // Read before the properties are built so that they are published with the controller's own values
    // rather than with zeros that get corrected a moment later.
    readAllParameters();
    buildParameterGroups();

    m_capTarget   = CapTarget::None;
    m_failedPolls = 0;

    // Where each mechanism already is counts as where it was asked to be, so the first poll after
    // connecting does not report a stall against a target nobody set.
    if (m_roles.hasFocuser())
    {
        m_focusTarget =
            static_cast<double>(m_device->status().motor(m_roles.focuserMotor().value()).position) / m_stepMultiplier;
    }

    if (m_roles.hasRotator())
    {
        m_rotatorTarget = skyOf(mechanicalDegreesOf(m_device->status().motor(m_roles.rotatorMotor().value()).position));
    }

    return true;
}

void ScopeLink::refreshMotorRoles()
{
    if (!isReady() || !m_roles.isConfigurable())
        return;

    try
    {
        m_roles = m_device->refreshMotorRoles();
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The motor assignment could not be read back: %s", error.what());
        return;
    }

    LOGF_INFO("Motor assignment: %s.", m_roles.toString().c_str());

    adoptMotorRoles();
    buildCalibrationChannels();

    // Nothing here rebuilds the devices, and a reconnection would not be enough on its own either. The
    // controller reads its configuration when it starts and drives from that copy, so until it restarts
    // it will keep using the assignment it booted with - and would refuse a command for a function the
    // stored assignment names but the running one does not. Measured on a generation 3 unit on interface
    // 1.1: a rotator assigned while the controller ran was refused as "no motor is assigned to it" until
    // the controller was restarted, after which the same command worked.
    LOG_WARN("The motor assignment has been changed in the controller's configuration, but the controller "
             "applies it when it starts: restart it with 'Restart controller' on this tab. Until then it "
             "goes on driving the assignment it started with, and will refuse a command for anything the "
             "new one adds.");
}

bool ScopeLink::adoptMotorRoles()
{
    m_focuserTravel      = 0;
    m_flapTravel         = 0;
    m_rotatorTravel      = 0;
    m_stepsPerRevolution = 1;

    // An assignment the controller will not act on is one this driver does not act on either. The firmware
    // checks the same three rules and is already reporting every function as unconfigured, so offering a
    // focuser here would offer one that answers every command with a refusal. Everything that does not
    // depend on the assignment - the fans, the outputs, the telemetry, and the parameter tab that is how
    // this gets corrected - carries on working.
    if (m_roles.isConfigurable() && !m_roles.isValid())
    {
        LOGF_ERROR("The controller's motor assignment is unusable: %s. No focuser, rotator or front flap "
                   "is offered until it is corrected on the Parameters tab.",
                   m_roles.problem().c_str());

        return true;
    }

    // The step multiplier is the controller's from interface 1.1 and this driver's before it. Held in the
    // controller so that both drivers read one number: a focuser step then means the same thing to a
    // client on either platform, which it did not when each of them kept its own.
    if (m_roles.isConfigurable())
    {
        m_stepMultiplier = m_roles.focuserStepMultiplier();

        StepMultiplierNP[0].setValue(m_stepMultiplier);
        StepMultiplierNP.setState(IPS_OK);
        StepMultiplierNP.apply();
    }

    if (m_roles.hasFocuser())
    {
        if (!readMotorTravel(scopelink::motor::maximumPositionDid(m_roles.focuserMotor().value()), m_focuserTravel)
            || (m_focuserTravel <= 0))
        {
            LOG_ERROR("The focuser's calibrated travel could not be read from the controller.");
            return false;
        }

        // Reported rather than refused, unlike the flap. The driver never drives the focuser to its
        // maximum on its own, so an uncalibrated one only reaches an impossible position if a client asks
        // for one, and everywhere inside the physical travel still works. Refusing here would take a
        // working focuser away from anyone whose unit was never calibrated.
        if (m_focuserTravel == scopelink::motor::UncalibratedTravel)
        {
            LOG_WARN("The focuser has not been calibrated. The controller holds no end of travel for it, so "
                     "the maximum position below is the uncalibrated placeholder and nothing stops a client "
                     "asking for a position past the physical travel. Calibrate it on the Calibration tab.");
        }

        const int clientTravel = m_focuserTravel / m_stepMultiplier;

        FocusMaxPosNP[0].setValue(clientTravel);
        FocusMaxPosNP[0].setMinMax(0, clientTravel);
        FocusAbsPosNP[0].setMinMax(0, clientTravel);
        FocusRelPosNP[0].setMinMax(0, clientTravel);
        FocusSyncNP[0].setMinMax(0, clientTravel);

        LOGF_INFO("Focuser travel is %d controller steps on motor %d, %d client steps at a multiplier of %d.",
                  m_focuserTravel, m_roles.focuserMotor().value() + 1, clientTravel, m_stepMultiplier);
    }
    else
    {
        LOG_INFO("No motor is assigned to the focuser on this controller, so no focuser is offered.");
    }

    if (m_roles.hasRotator())
    {
        m_stepsPerRevolution = m_roles.rotatorStepsPerRevolution();

        if (!readMotorTravel(scopelink::motor::maximumPositionDid(m_roles.rotatorMotor().value()), m_rotatorTravel)
            || (m_rotatorTravel <= 0))
        {
            LOG_ERROR("The rotator's calibrated travel could not be read from the controller.");
            return false;
        }

        // Refused as a device rather than offered with a placeholder travel, unlike the focuser. Every
        // angle a client asks for is turned into a step count against this number, so an uncalibrated
        // rotator would accept an angle and drive towards a position two billion steps away.
        if (m_rotatorTravel == scopelink::motor::UncalibratedTravel)
        {
            LOG_WARN("The field rotator has not been calibrated, so it is not offered. Calibrate it on the "
                     "Calibration tab, then reconnect.");

            m_rotatorTravel = 0;
        }
        else
        {
            LOGF_INFO("Rotator travel is %d controller steps on motor %d, %.1f degrees at %ld steps per "
                      "revolution.",
                      m_rotatorTravel, m_roles.rotatorMotor().value() + 1,
                      (m_rotatorTravel * 360.0) / m_stepsPerRevolution, m_stepsPerRevolution);
        }
    }

    if (!m_roles.hasFlap())
        return true;

    // Every part, not only the first. A flap of two parts with one of them uncalibrated cannot be opened
    // any more than a flap with neither of them calibrated can, and the part that decides the position
    // shown is the one that opens first.
    for (size_t part = 0; part < m_roles.flapMotors().size(); part++)
    {
        int travel = 0;

        if (!readMotorTravel(scopelink::motor::maximumPositionDid(m_roles.flapMotors()[part]), travel) || (travel <= 0))
        {
            LOG_ERROR("The front flap's calibrated travel could not be read from the controller.");
            return false;
        }

        // An uncalibrated flap is not offered as a cover at all. Unlike the focuser this does not stop the
        // connection, because everything else on the controller still works - but it must not be published
        // as a dust cap, because a dust cap is a thing Ekos opens and closes on its own schedule, and the
        // only move this one could make is towards a position two billion steps away.
        if (travel == scopelink::motor::UncalibratedTravel)
        {
            LOGF_WARN("Part %zu of the front flap has not been calibrated, so the flap is not offered as a "
                      "dust cap. Calibrate it on the Calibration tab, then reconnect.",
                      part + 1);

            m_flapTravel = 0;
            return true;
        }

        if (part == 0)
            m_flapTravel = travel;
    }

    CapPositionNP[0].setMinMax(0, m_flapTravel);

    LOGF_INFO("Front flap travel is %d controller steps, on %zu part(s).", m_flapTravel, m_roles.flapMotors().size());

    return true;
}

bool ScopeLink::Disconnect()
{
    if (m_update)
    {
        // Refused rather than obeyed. From the erase onwards the controller has no firmware until the
        // download finishes, and dropping the port in the middle of that leaves the user with a unit that
        // has to be recovered rather than one that is merely disconnected.
        LOG_ERROR("A firmware update is running. Disconnecting now would leave the ScopeLink without "
                  "firmware; wait for the update to finish.");

        return false;
    }

    // A restart that is still waiting is abandoned rather than left to finish. Its last act is to
    // connect again, and connecting again is the one thing a user who has just asked to be disconnected
    // is not asking for.
    m_restarting = false;

    // The descriptor belongs to libindi's connection plugin, which closes it as part of the call below.
    releaseDevice();
    clearPortExclusivity();

    return INDI::DefaultDevice::Disconnect();
}

void ScopeLink::reportLinkLost(const std::string &reason)
{
    LOGF_ERROR("Lost contact with the ScopeLink controller: %s", reason.c_str());

    releaseDevice();

    // Released explicitly rather than left to the next connect: the descriptor points at a device that
    // is very likely no longer on the bus, and holding it open stops the kernel from cleanly
    // re-enumerating the controller when it comes back.
    if (m_serialConnection != nullptr)
    {
        clearPortExclusivity();
        m_serialConnection->Disconnect();
    }

    setConnected(false, IPS_ALERT);
    updateProperties();
}

bool ScopeLink::readMotorTravel(uint32_t maximumPositionDid, int &travel)
{
    const scopelink::DidDescriptor *descriptor = scopelink::findDidDescriptor(maximumPositionDid);

    if (descriptor == nullptr)
    {
        LOGF_ERROR("The firmware description this driver was built from has no identifier 0x%04X.", maximumPositionDid);
        return false;
    }

    scopelink::Did identifier(*descriptor);

    if (!identifier.read(*m_device))
    {
        LOGF_ERROR("%s", identifier.lastError().c_str());
        return false;
    }

    travel = identifier.value();

    return true;
}

// ---------------------------------------------------------------------------------------------------
// Restarting the controller
// ---------------------------------------------------------------------------------------------------

void ScopeLink::restartController()
{
    if (m_update)
    {
        LOG_ERROR("A firmware update is running. It restarts the controller itself when it finishes.");
        return;
    }

    if (m_restarting)
    {
        LOG_WARN("The ScopeLink is already restarting.");
        return;
    }

    if (!isReady())
    {
        LOG_ERROR("There is no controller to restart.");
        return;
    }

    if (isSimulation())
    {
        // The request is real - the simulated controller reads its stored assignment again, which is the
        // whole of what a restart changes. What does not follow is the reconnection: which devices this
        // driver publishes is settled when it connects, and connecting in simulation builds a new
        // simulated controller with the default configuration, which would throw away the very assignment
        // the restart was for.
        m_device->requestReset();

        LOG_INFO("The simulated controller has read its stored motor assignment again. Which devices this "
                 "driver publishes is decided when it connects, and connecting in simulation builds a "
                 "fresh simulated controller - so to watch an assignment change take effect, run "
                 "scopelink-simulator and point the driver at the port it serves.");

        RestartSP.setState(IPS_OK);
        RestartSP.apply();

        return;
    }

    LOG_WARN("Restarting the ScopeLink. The link drops while it leaves the USB bus and comes back, which "
             "takes a few seconds.");

    // Reported rather than thrown by the device layer, and deliberately not acted on either way: a
    // controller that restarted before its answer got out looks exactly like one that never heard the
    // request, and the wait below is what tells them apart.
    if (!m_device->requestReset())
    {
        LOG_WARN("The ScopeLink did not acknowledge the restart request. Waiting to see whether it "
                 "restarts anyway.");
    }

    m_restartPort = m_serialConnection->port();

    // Everything built on the descriptor, and then the descriptor. It points at a device that is about to
    // leave the bus, and holding it open stops the kernel from cleanly enumerating the controller when it
    // comes back.
    releaseDevice();
    clearPortExclusivity();
    m_serialConnection->Disconnect();

    const auto now = std::chrono::steady_clock::now();

    m_restarting       = true;
    m_restartDeadline  = now + std::chrono::milliseconds(RestartTimeoutMs);
    m_restartNextProbe = now + std::chrono::milliseconds(RestartProbeIntervalMs);

    RestartSP.setState(IPS_BUSY);
    RestartSP.apply();
}

void ScopeLink::clearPortExclusivity()
{
    if (m_serialConnection == nullptr)
        return;

    const int descriptor = m_serialConnection->getPortFD();

    if (descriptor < 0)
        return;

    // Not reported when it fails. A port that will not give up a claim it may never have made is not
    // something the user can do anything about, and the open that follows says so much more clearly.
    ioctl(descriptor, TIOCNXCL);
}

void ScopeLink::tickControllerRestart()
{
    const auto now = std::chrono::steady_clock::now();

    if (now < m_restartNextProbe)
        return;

    m_restartNextProbe = now + std::chrono::milliseconds(RestartProbeIntervalMs);

    if (controllerAnswers())
    {
        m_restarting = false;

        LOG_INFO("The ScopeLink has restarted. Connecting again, so that the devices follow the assignment "
                 "it has just read.");

        RestartSP.setState(IPS_OK);
        RestartSP.apply();

        reopenController("The ScopeLink answered after its restart, but the driver could not open it "
                         "again. Connect again when it is ready.");

        return;
    }

    if (now < m_restartDeadline)
        return;

    m_restarting = false;

    LOGF_ERROR("The ScopeLink was restarted but did not come back on %s within %d seconds. Unplug it and "
               "plug it back in, then connect again. If it comes back under a different port name, point "
               "the driver at its /dev/serial/by-id path instead, which does not change.",
               m_restartPort.c_str(), RestartTimeoutMs / 1000);

    RestartSP.setState(IPS_ALERT);
    RestartSP.apply();

    setConnected(false, IPS_ALERT);
    updateProperties();
}

bool ScopeLink::controllerAnswers()
{
    scopelink::PosixSerialTransport port(m_restartPort, scopelink::Device::ReceiveTimeoutMs);

    if (!port.reopen())
    {
        LOGF_DEBUG("The restarted ScopeLink's port is not there yet: %s", port.lastError().c_str());
        return false;
    }

    try
    {
        scopelink::Protocol protocol(port);
        scopelink::Device device(protocol);

        device.open();
    }
    catch (const std::exception &error)
    {
        // Every reason is the same reason here: it is not back yet. Worth having in the log at debug
        // level and nowhere else, because a restart takes several seconds and this runs twice a second
        // for every one of them.
        LOGF_DEBUG("The restarted ScopeLink has not answered yet: %s", error.what());
        return false;
    }

    return true;
}

void ScopeLink::reopenController(const char *failure)
{
    setConnected(false, IPS_IDLE);
    updateProperties();

    if (m_serialConnection->Connect())
    {
        setConnected(true, IPS_OK);
        updateProperties();

        return;
    }

    LOGF_ERROR("%s", failure);

    setConnected(false, IPS_ALERT);
    updateProperties();
}

// ---------------------------------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------------------------------

void ScopeLink::TimerHit()
{
    if (!isConnected())
        return;

    if (m_update)
    {
        tickFirmwareUpdate();

        // A finished update reconnects, and the reconnect arms the timer itself through
        // updateProperties(); arming a second one here would leave two of them running for the rest of
        // the session, each re-arming the other.
        if (m_update)
            SetTimer(getCurrentPollingPeriod());

        return;
    }

    if (m_restarting)
    {
        // Nothing is polled while the controller is away: there is no port open, and the poll's own idea
        // of a controller that has stopped answering would take the device down three polls into a
        // restart that has thirty seconds to finish in.
        tickControllerRestart();

        // A finished restart connects again, and connecting arms the timer itself through
        // updateProperties(); arming a second one here would leave two of them running for the rest of
        // the session, each re-arming the other.
        if (m_restarting)
            SetTimer(getCurrentPollingPeriod());

        return;
    }

    if (m_bootloaderOnly)
    {
        // There is no firmware to poll. The timer keeps running so that the connection stays alive for
        // the one thing this state is good for, which is installing firmware.
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    try
    {
        const scopelink::Status &status = m_device->refreshStatus();

        m_failedPolls = 0;

        publishFocuser(status);
        publishRotator(status);
        publishCap(status);
        publishLightBox(status);
        publishTelemetry(status);
        publishFans(status);
        publishPower(status);
        publishCalibration(status);
        publishLinkHealth();
    }
    catch (const std::exception &error)
    {
        m_failedPolls++;

        // One lost poll is normal on a busy USB bus and the protocol layer has already retried it three
        // times. Three lost polls in a row is a controller that has stopped answering, and continuing to
        // poll it would only fill the log - the user needs to be told and the device taken down.
        if (m_failedPolls >= 3)
        {
            reportLinkLost(error.what());
            return;
        }

        LOGF_DEBUG("Status poll failed (%d in a row): %s", m_failedPolls, error.what());
    }

    SetTimer(getCurrentPollingPeriod());
}

void ScopeLink::publishFocuser(const scopelink::Status &status)
{
    if (!m_roles.hasFocuser())
    {
        // No focuser, but possibly still a temperature sensor: the reading is the controller's rather than
        // the focuser's, and a client watching it has no interest in which motors are assigned to what.
        if (m_device->capabilities().hasTemperatureSensor)
        {
            FocusTemperatureNP[0].setValue(status.ambientTemperatureCelsius());
            FocusTemperatureNP.setState(status.ambientTemperatureValid ? IPS_OK : IPS_ALERT);
            FocusTemperatureNP.apply();
        }

        return;
    }

    const scopelink::MotorReading focuser = status.motor(m_roles.focuserMotor().value());
    const double position                 = static_cast<double>(focuser.position) / m_stepMultiplier;

    FocusAbsPosNP[0].setValue(position);

    // A stopped motor is not on its own proof that a move has finished: the controller has not
    // necessarily started by the time the poll after the command lands. Requiring the position as well
    // removes that race, and it turns a move that stopped short - the stall detection doing its job -
    // into an alert rather than a silently wrong "arrived".
    if ((FocusAbsPosNP.getState() == IPS_BUSY) && !focuser.moving)
    {
        const bool arrived = std::abs(position - m_focusTarget) <= 1.0;

        if (!arrived && (m_focusSettle > 0))
        {
            // The command has been sent but the controller has not reported motion yet. Concluding
            // anything from this poll would report a stall on a move that has not started.
            m_focusSettle--;
        }
        else
        {
            FocusAbsPosNP.setState(arrived ? IPS_OK : IPS_ALERT);
            FocusRelPosNP.setState(arrived ? IPS_OK : IPS_ALERT);
            FocusRelPosNP.apply();

            if (arrived)
            {
                LOGF_INFO("Focuser arrived at %.0f.", position);
            }
            else
            {
                LOGF_ERROR("The focuser stopped at %.0f instead of %.0f. It may be obstructed, or the stall "
                           "detection sensitivity may be set too high.",
                           position, m_focusTarget);
            }
        }
    }

    FocusAbsPosNP.apply();

    if (m_device->capabilities().hasTemperatureSensor)
    {
        // A failed infrared sensor reports a raw value that converts to a plausible looking -273 degrees,
        // so a reading that is not believable is published as an alert rather than as a number.
        if (status.ambientTemperatureValid)
        {
            FocusTemperatureNP[0].setValue(status.ambientTemperatureCelsius());
            FocusTemperatureNP.setState(IPS_OK);
        }
        else
        {
            FocusTemperatureNP.setState(IPS_ALERT);
        }

        FocusTemperatureNP.apply();
    }
}

void ScopeLink::publishRotator(const scopelink::Status &status)
{
    if (!m_roles.hasRotator() || (m_rotatorTravel <= 0))
        return;

    const scopelink::MotorReading rotator = status.motor(m_roles.rotatorMotor().value());
    const double sky                      = skyOf(mechanicalDegreesOf(rotator.position));

    GotoRotatorNP[0].setValue(sky);

    if ((GotoRotatorNP.getState() == IPS_BUSY) && !rotator.moving)
    {
        // Half a degree, rather than the exact angle. A step is 360 degrees divided by the steps in a
        // revolution and the target was rounded to a whole one of them, so an arrival is never exact - and
        // an angle that is a step out is an arrival, where one that is ten degrees out is a stall.
        const double error = std::abs(normaliseAngle(sky - m_rotatorTarget + 180.0) - 180.0);
        const bool arrived = error <= 0.5;

        if (!arrived && (m_rotatorSettle > 0))
        {
            // As with the focuser: the command is out but the controller has not reported motion yet.
            m_rotatorSettle--;
        }
        else
        {
            GotoRotatorNP.setState(arrived ? IPS_OK : IPS_ALERT);

            if (arrived)
                LOGF_INFO("Rotator arrived at %.2f degrees.", sky);
            else
                LOGF_ERROR("The rotator stopped at %.2f degrees instead of %.2f. It may be obstructed, or the "
                           "stall detection sensitivity may be set too high.",
                           sky, m_rotatorTarget);
        }
    }

    GotoRotatorNP.apply();
}

void ScopeLink::publishTelemetry(const scopelink::Status &status)
{
    RailsNP[0].setValue(status.supplyVoltage / 1000.0);
    RailsNP[1].setValue(status.fanAVoltage / 1000.0);
    RailsNP[2].setValue(status.fanBVoltage / 1000.0);
    RailsNP.setState(IPS_OK);
    RailsNP.apply();

    for (size_t index = 0; index < MotorLoadNP.size(); index++)
        MotorLoadNP[index].setValue(status.motor(static_cast<int>(index)).load);

    MotorLoadNP.setState(IPS_OK);
    MotorLoadNP.apply();

    if (m_device->capabilities().hasTemperatureSensor)
    {
        const bool usable = status.ambientTemperatureValid && status.mirrorTemperatureValid;

        if (usable)
        {
            TemperatureNP[0].setValue(status.ambientTemperatureCelsius());
            TemperatureNP[1].setValue(status.mirrorTemperatureCelsius());
            TemperatureNP[2].setValue(status.mirrorTemperatureCelsius() - status.ambientTemperatureCelsius());
        }

        TemperatureNP.setState(usable ? IPS_OK : IPS_ALERT);
        TemperatureNP.apply();
    }

    FaultCountNP[0].setValue(status.storedFaultCount);
    FaultCountNP[1].setValue(status.activeFaultCount);
    FaultCountNP.setState((status.activeFaultCount > 0) ? IPS_ALERT :
                                                          ((status.storedFaultCount > 0) ? IPS_BUSY : IPS_OK));
    FaultCountNP.apply();

    if (DeveloperSP[0].getState() != ISS_ON)
        return;

    ControllerNP[0].setValue(status.sensorSupplyVoltage / 1000.0);
    ControllerNP[1].setValue(status.controllerSupplyVoltage / 1000.0);
    ControllerNP[2].setValue(status.controllerTemperature);
    ControllerNP[3].setValue(status.cpuLoad);
    ControllerNP[4].setValue(status.peakCpuLoad);
    ControllerNP[5].setValue(status.stackUsage);
    ControllerNP[6].setValue(status.i2cErrorCounter);
    ControllerNP.setState(IPS_OK);
    ControllerNP.apply();
}

void ScopeLink::publishFans(const scopelink::Status &status)
{
    FanOverrideSP[0].setState(status.fanAManualOverrideEnabled ? ISS_ON : ISS_OFF);
    FanOverrideSP[1].setState(status.fanBManualOverrideEnabled ? ISS_ON : ISS_OFF);
    FanOverrideSP.setState(IPS_OK);
    FanOverrideSP.apply();

    FanStateSP[0].setState(status.fanAManualOverrideState ? ISS_ON : ISS_OFF);
    FanStateSP[1].setState(status.fanBManualOverrideState ? ISS_ON : ISS_OFF);
    FanStateSP.setState(IPS_OK);
    FanStateSP.apply();

    if (m_device->capabilities().hasTemperatureSensor)
    {
        FanTargetNP[0].setValue(status.fanATargetDT / 50.0);
        FanTargetNP[1].setValue(status.fanBTargetDT / 50.0);
        FanTargetNP.setState(IPS_OK);
        FanTargetNP.apply();
    }
}

void ScopeLink::publishPower(const scopelink::Status &status)
{
    const scopelink::Capabilities &capabilities = m_device->capabilities();

    AuxPowerSP[0].setState(status.powerSwitch1State ? ISS_ON : ISS_OFF);
    AuxPowerSP[1].setState(status.powerSwitch2State ? ISS_ON : ISS_OFF);
    AuxPowerSP.setState(IPS_OK);
    AuxPowerSP.apply();

    if (capabilities.hasUsbHub)
    {
        const int ports = status.usbDownstreamPortCount();

        for (int port = 0; port < ports; port++)
            UsbHubLP[port].setState(status.usbPowerActive(port) ? IPS_OK : IPS_IDLE);

        // The fault lamps exist only where the controller reports them, and sizeHardwareProperties made
        // exactly as many as it does - so the group is as wide as the ports alone on a controller with
        // none, and the loop above has already filled all of it.
        const bool faulted = status.usb1PowerFailure || status.usb2PowerFailure;

        if (static_cast<int>(UsbHubLP.size()) > ports)
        {
            UsbHubLP[ports].setState(status.usb1PowerFailure ? IPS_ALERT : IPS_IDLE);
            UsbHubLP[ports + 1].setState(status.usb2PowerFailure ? IPS_ALERT : IPS_IDLE);
        }

        UsbHubLP.setState(faulted ? IPS_ALERT : IPS_OK);
        UsbHubLP.apply();
    }
}

// ---------------------------------------------------------------------------------------------------
// Focuser interface
// ---------------------------------------------------------------------------------------------------

IPState ScopeLink::MoveAbsFocuser(uint32_t targetTicks)
{
    if (!isReady() || !m_roles.hasFocuser())
        return IPS_ALERT;

    const uint32_t limit = static_cast<uint32_t>(m_focuserTravel / m_stepMultiplier);

    if (targetTicks > limit)
    {
        LOGF_ERROR("Rejected position %u, the allowed range is 0 to %u.", targetTicks, limit);
        return IPS_ALERT;
    }

    // The range check above is what keeps this conversion safe: the limit is itself the controller's own
    // travel divided by the multiplier, so anything accepted multiplies back to a position the controller
    // can reach.
    const long native = static_cast<long>(targetTicks) * m_stepMultiplier;

    try
    {
        // Stopped first when it is already moving. The motor driver will not take a new target while it is
        // running - a controller with the function commands says so, and one without it says nothing at
        // all - and a client changing its mind mid-move is asking for the new target, not for a refusal.
        if (m_device->requireStatus().motor(m_roles.focuserMotor().value()).moving)
            driveFocuser(scopelink::function::Halt, 0);

        driveFocuser(scopelink::function::Move, static_cast<int>(native));
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the move: %s", error.what());
        return IPS_ALERT;
    }

    m_focusTarget = targetTicks;
    m_focusSettle = 1;

    LOGF_DEBUG("Moving to %u (%ld controller steps).", targetTicks, native);

    return IPS_BUSY;
}

IPState ScopeLink::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    const int32_t current = static_cast<int32_t>(FocusAbsPosNP[0].getValue());
    const int32_t target =
        (dir == FOCUS_INWARD) ? (current - static_cast<int32_t>(ticks)) : (current + static_cast<int32_t>(ticks));

    if (target < 0)
    {
        LOGF_ERROR("A relative move of %u steps inward would pass zero, which is the end of the travel.", ticks);
        return IPS_ALERT;
    }

    return MoveAbsFocuser(static_cast<uint32_t>(target));
}

bool ScopeLink::AbortFocuser()
{
    if (!isReady() || !m_roles.hasFocuser())
        return false;

    try
    {
        driveFocuser(scopelink::function::Halt, 0);
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        // Deliberately reported rather than swallowed. Anyone calling abort is very likely trying to stop
        // a move that has gone wrong, which is the worst possible moment to be told everything is fine.
        LOGF_ERROR("The controller did not accept the abort: %s", error.what());
        return false;
    }

    // Wherever it stopped is now where it was asked to be, so an abort does not leave the next poll
    // reporting a stall against the target the user just cancelled.
    m_focusTarget =
        static_cast<double>(m_device->status().motor(m_roles.focuserMotor().value()).position) / m_stepMultiplier;

    LOG_INFO("Focuser motor halted.");

    return true;
}

bool ScopeLink::SyncFocuser(uint32_t ticks)
{
    if (!isReady() || !m_roles.hasFocuser())
        return false;

    try
    {
        driveFocuser(scopelink::function::Sync, static_cast<int>(static_cast<long>(ticks) * m_stepMultiplier));
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the sync: %s", error.what());
        return false;
    }

    m_focusTarget = ticks;

    return true;
}

bool ScopeLink::SetFocuserMaxPosition(uint32_t ticks)
{
    // Restoring a saved configuration writes this property back at whatever was saved, which is not a user
    // asking for anything - and treating it as one puts a red error in the log on every single connect.
    // Both of those cases are accepted quietly; the controller's own travel is published over them a
    // moment later either way.
    if (!isReady() || !m_roles.hasFocuser() || (ticks == static_cast<uint32_t>(m_focuserTravel / m_stepMultiplier)))
        return true;

    // The travel limit is the controller's calibration, not a client preference. Letting the focus module
    // write it would let a stray number destroy a calibration that took a bench session to establish, and
    // the client has no way of knowing what the mechanics can actually reach.
    LOG_ERROR("The focuser travel comes from the controller's calibration and cannot be set from here. "
              "Use the Calibration tab to recalibrate it.");

    return false;
}

// ---------------------------------------------------------------------------------------------------
// Rotator interface
// ---------------------------------------------------------------------------------------------------

double ScopeLink::normaliseAngle(double degrees)
{
    double wrapped = std::fmod(degrees, 360.0);

    if (wrapped < 0.0)
        wrapped += 360.0;

    // An angle a hair below zero comes back out of the addition as exactly 360, which is the one value
    // the range excludes.
    if (wrapped >= 360.0)
        wrapped = 0.0;

    return wrapped;
}

double ScopeLink::skyOf(double mechanical) const
{
    return normaliseAngle((m_rotatorReverse ? -mechanical : mechanical) + m_rotatorSyncOffset);
}

double ScopeLink::mechanicalOf(double sky) const
{
    // The inverse of skyOf, which is its own inverse in the sign because the sign is plus or minus one.
    const double difference = sky - m_rotatorSyncOffset;

    return normaliseAngle(m_rotatorReverse ? -difference : difference);
}

double ScopeLink::mechanicalDegreesOf(int steps) const
{
    // The remainder is taken before the division rather than after it: a rotator whose travel is more than
    // one turn reaches step counts where the multiplication would lose the last few steps to the width of
    // a double, and the answer is wanted to better than a step.
    long within = steps % m_stepsPerRevolution;

    if (within < 0)
        within += m_stepsPerRevolution;

    return normaliseAngle((within * 360.0) / m_stepsPerRevolution);
}

long ScopeLink::rotatorStepsFor(double mechanical, int current) const
{
    long within = std::lround((mechanical / 360.0) * static_cast<double>(m_stepsPerRevolution)) % m_stepsPerRevolution;

    if (within < 0)
        within += m_stepsPerRevolution;

    if (within > m_rotatorTravel)
        return -1;

    // Every step count congruent to the angle is that angle. The reachable one nearest to where the motor
    // already is is the shortest movement, and the one that does not unwind a cable it has just wound up.
    long best = within;

    for (long candidate = within; candidate <= m_rotatorTravel; candidate += m_stepsPerRevolution)
    {
        if (std::labs(candidate - current) < std::labs(best - current))
            best = candidate;
    }

    return best;
}

IPState ScopeLink::MoveRotator(double angle)
{
    if (!isReady() || !m_roles.hasRotator() || (m_rotatorTravel <= 0))
        return IPS_ALERT;

    const double sky = normaliseAngle(angle);

    try
    {
        const scopelink::MotorReading rotator = m_device->requireStatus().motor(m_roles.rotatorMotor().value());
        const long steps                      = rotatorStepsFor(mechanicalOf(sky), rotator.position);

        // Reported rather than approximated to the nearest end stop. Driving there would leave a client
        // believing it had rotated the camera when it had not.
        if (steps < 0)
        {
            LOGF_ERROR("The rotator cannot reach %.2f degrees: its travel is %.1f degrees and stops short of "
                       "it.",
                       sky, (m_rotatorTravel * 360.0) / m_stepsPerRevolution);
            return IPS_ALERT;
        }

        // Stopped first when it is already turning, exactly as the focuser is and for the same reason: the
        // motor driver will not take a new target while it is running, and a client changing its mind
        // mid-rotation is asking for the new target rather than for a refusal.
        if (rotator.moving)
            m_device->haltRotator();

        m_device->moveRotator(static_cast<int>(steps));
        m_device->refreshStatus();

        LOGF_DEBUG("Rotating to %.2f degrees on the sky, %.2f mechanical, %ld steps from %d.", sky, mechanicalOf(sky),
                   steps, rotator.position);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the rotation: %s", error.what());
        return IPS_ALERT;
    }

    m_rotatorTarget = sky;
    m_rotatorSettle = 1;

    return IPS_BUSY;
}

/**
 * @brief Adopts a sky angle for where the rotator is standing now.
 *
 * Nothing is sent to the controller. The controller does have a rotator sync, and using it here would be
 * wrong twice over: it moves the motor's step count, which would change the mechanical angle this member
 * is specified to leave alone, and it would break the relationship between step zero and the end of
 * travel that every range check here rests on. What a sync means is that the mechanism is where it is and
 * the sky is somewhere else than the driver thought, so what it changes is the offset between them.
 */
bool ScopeLink::SyncRotator(double angle)
{
    if (!isReady() || !m_roles.hasRotator() || (m_rotatorTravel <= 0))
        return false;

    double mechanical = 0;

    try
    {
        mechanical = mechanicalDegreesOf(m_device->requireStatus().motor(m_roles.rotatorMotor().value()).position);
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The rotator's position could not be read, so it was not synced: %s", error.what());
        return false;
    }

    const double sky = normaliseAngle(angle);

    m_rotatorSyncOffset = normaliseAngle(sky - (m_rotatorReverse ? -mechanical : mechanical));
    m_rotatorTarget     = sky;

    RotatorOffsetNP[0].setValue(m_rotatorSyncOffset);
    RotatorOffsetNP.setState(IPS_OK);
    RotatorOffsetNP.apply();

    // Saved at once rather than at shutdown, because it outlasts the connection on purpose: a sky
    // calibration that had to be redone every time a client reconnected would not be worth doing at all.
    saveConfig(true, RotatorOffsetNP.getName());

    LOGF_INFO("The mechanism is at %.2f degrees and has been told that is %.2f degrees on the sky, an offset "
              "of %.2f degrees.",
              mechanical, sky, m_rotatorSyncOffset);

    return true;
}

bool ScopeLink::ReverseRotator(bool enabled)
{
    if (m_rotatorReverse == enabled)
        return true;

    m_rotatorReverse = enabled;

    // The offset was measured with the angle running the other way, so it now describes a different sky.
    // Said rather than silently recomputed: only the user knows what the camera is actually pointing at.
    LOGF_WARN("The rotator angle now runs %s the mechanism. The sync offset was measured the other way "
              "round, so sync it again before relying on the angle.",
              enabled ? "opposite to" : "with");

    return true;
}

bool ScopeLink::AbortRotator()
{
    if (!isReady() || !m_roles.hasRotator())
        return false;

    try
    {
        m_device->haltRotator();
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        // Deliberately reported rather than swallowed, as the focuser's abort is: anyone calling abort is
        // very likely trying to stop a move that has gone wrong.
        LOGF_ERROR("The controller did not accept the abort: %s", error.what());
        return false;
    }

    // Wherever it stopped is now where it was asked to be, so the next poll does not report a stall
    // against the target the user just cancelled.
    m_rotatorTarget = skyOf(mechanicalDegreesOf(m_device->status().motor(m_roles.rotatorMotor().value()).position));

    LOG_INFO("Rotator motor halted.");

    return true;
}

// ---------------------------------------------------------------------------------------------------
// Cover
// ---------------------------------------------------------------------------------------------------

IPState ScopeLink::moveCap(CapTarget target)
{
    try
    {
        // A flap that is already running is stopped first. Commanding a new target while the previous one
        // is still being followed is not something the controller is specified to accept, and a controller
        // with the function commands answers such a request with Busy.
        if (m_device->requireStatus().motor(m_roles.flapMotors().front()).moving)
            driveFlap(scopelink::function::Halt);

        driveFlap((target == CapTarget::Park) ? scopelink::function::Close : scopelink::function::Open);
        m_device->refreshStatus();
    }
    catch (const std::exception &error)
    {
        LOGF_ERROR("The controller did not accept the flap command: %s", error.what());
        return IPS_ALERT;
    }

    m_capTarget = target;
    m_capSettle = 1;

    LOGF_INFO("Front flap %s.", (target == CapTarget::Park) ? "closing" : "opening");

    return IPS_BUSY;
}

void ScopeLink::driveFocuser(uint8_t subFunction, int position)
{
    if (commandsFunctions())
    {
        if (subFunction == scopelink::function::Move)
            m_device->moveFocuser(position);
        else if (subFunction == scopelink::function::Sync)
            m_device->syncFocuser(position);
        else
            m_device->haltFocuser();

        return;
    }

    const int motorId = m_roles.focuserMotor().value();

    if (subFunction == scopelink::function::Move)
        m_device->moveMotor(motorId, position);
    else if (subFunction == scopelink::function::Sync)
        m_device->syncMotor(motorId, position);
    else
        m_device->haltMotor(motorId);
}

void ScopeLink::driveFlap(uint8_t subFunction)
{
    if (commandsFunctions())
    {
        if (subFunction == scopelink::function::Open)
            m_device->openFlap();
        else if (subFunction == scopelink::function::Close)
            m_device->closeFlap();
        else
            m_device->haltFlap();

        return;
    }

    // One motor, driven to one end of one travel, because a flap of several parts is a thing only a
    // controller with the function commands can have.
    const int motorId = m_roles.flapMotors().front();

    if (subFunction == scopelink::function::Halt)
        m_device->haltMotor(motorId);
    else
        m_device->moveMotor(motorId, (subFunction == scopelink::function::Close) ? 0 : m_flapTravel);
}

const char *ScopeLink::flapStateName(scopelink::FlapState state)
{
    switch (state)
    {
        case scopelink::FlapState::Closed:
            return "Closed";

        case scopelink::FlapState::Opening:
            return "Opening";

        case scopelink::FlapState::Open:
            return "Open";

        case scopelink::FlapState::Closing:
            return "Closing";

        case scopelink::FlapState::Partial:
            return "Stopped part way";

        case scopelink::FlapState::NotConfigured:
            return "Not fitted";

        case scopelink::FlapState::NotCalibrated:
            return "Not calibrated";

        case scopelink::FlapState::Error:
            return "Error";

        default:
            return "Not reported";
    }
}

void ScopeLink::adoptCapStateFromPosition(const scopelink::Status &status)
{
    const int tolerance = std::max(MinimumCapTolerance, m_flapTravel / 200);

    const int position = status.motor(m_roles.flapMotors().front()).position;

    CapParkSP.reset();

    if (std::abs(position) <= tolerance)
    {
        CapParkSP[0].setState(ISS_ON);
        CapParkSP.setState(IPS_OK);
    }
    else if (std::abs(position - m_flapTravel) <= tolerance)
    {
        CapParkSP[1].setState(ISS_ON);
        CapParkSP.setState(IPS_OK);
    }
    else
    {
        // Neither end and not moving. The Windows driver quietly closed the flap when it found this, which
        // is not a decision to make on a telescope nobody is standing next to - so it is reported and left
        // where it is, and the user decides.
        CapParkSP.setState(IPS_ALERT);

        LOGF_WARN("The front flap is at %d of %d steps, which is neither open nor closed. "
                  "It was probably interrupted mid-travel; close or open it to re-establish its state.",
                  position, m_flapTravel);
    }

    CapParkSP.apply();
}

/**
 * @brief Follows a flap the controller reports the state of.
 *
 * The state is what the controller says rather than what the positions imply, which is the whole reason
 * it is sent: a part standing still is opening while it waits out its delay, and a flap of two parts is
 * neither open nor shut until both of them have arrived. Nothing here compares a position to a travel.
 */
void ScopeLink::publishReportedCapState(const scopelink::Status &status)
{
    FlapStateTP[0].setText(flapStateName(status.flapState));
    FlapStateTP.setState((status.flapState == scopelink::FlapState::Error) ? IPS_ALERT : IPS_OK);
    FlapStateTP.apply();

    if (!hasUsableFlap())
        return;

    const bool moving =
        (status.flapState == scopelink::FlapState::Opening) || (status.flapState == scopelink::FlapState::Closing);

    if (moving)
    {
        CapParkSP.setState(IPS_BUSY);
        CapParkSP.apply();
        return;
    }

    // One poll of grace after the command, for the same reason the focuser has one: the controller has
    // been measured to report the sequence as started only in the frame after the one that follows it.
    if ((m_capTarget != CapTarget::None) && (m_capSettle > 0))
    {
        m_capSettle--;
        return;
    }

    const bool closed  = status.flapState == scopelink::FlapState::Closed;
    const bool open    = status.flapState == scopelink::FlapState::Open;
    const bool settled = closed || open;

    CapParkSP.reset();

    if (settled)
    {
        CapParkSP[closed ? 0 : 1].setState(ISS_ON);
        CapParkSP.setState(IPS_OK);

        if (m_capTarget != CapTarget::None)
        {
            const bool asked = closed == (m_capTarget == CapTarget::Park);

            if (asked)
                LOGF_INFO("Front flap %s.", closed ? "closed" : "open");
            else
                LOGF_ERROR("The front flap was asked to %s and reports itself %s.",
                           (m_capTarget == CapTarget::Park) ? "close" : "open", closed ? "closed" : "open");

            CapParkSP.setState(asked ? IPS_OK : IPS_ALERT);
        }
    }
    else
    {
        CapParkSP.setState(IPS_ALERT);

        if (m_capTarget != CapTarget::None)
            LOGF_ERROR("The front flap did not finish: the controller reports it as '%s'.",
                       flapStateName(status.flapState));
    }

    CapParkSP.apply();
    m_capTarget = CapTarget::None;
}

void ScopeLink::publishCap(const scopelink::Status &status)
{
    const bool reported = status.flapState != scopelink::FlapState::Unknown;

    // The flap state is published wherever the controller sends one, flap or no flap, because "this unit
    // has no front flap" is an answer worth showing.
    if (reported)
        publishReportedCapState(status);

    // Everything past here needs a flap, and a travel to measure its position against.
    if (!hasUsableFlap())
        return;

    const scopelink::MotorReading flap = status.motor(m_roles.flapMotors().front());

    CapPositionNP[0].setValue(flap.position);
    CapPositionNP[1].setValue(
        (m_flapTravel > 0) ? std::min(1.0, std::max(0.0, static_cast<double>(flap.position) / m_flapTravel)) : 0.0);
    CapPositionNP.setState(flap.moving ? IPS_BUSY : IPS_OK);
    CapPositionNP.apply();

    // A controller that reports its own flap state has already been followed above, and the position is
    // published beside it as the first part's own reading rather than as the state of the whole flap.
    if (reported)
        return;

    if (m_capTarget == CapTarget::None)
    {
        // Nothing has been commanded since the driver connected, so the flap's state is whatever the
        // position says it is.
        if (CapParkSP.getState() == IPS_IDLE)
            adoptCapStateFromPosition(status);

        return;
    }

    if (flap.moving)
        return;

    const int tolerance = std::max(MinimumCapTolerance, m_flapTravel / 200);
    const int target    = (m_capTarget == CapTarget::Park) ? 0 : m_flapTravel;
    const bool arrived  = std::abs(flap.position - target) <= tolerance;

    if (!arrived && (m_capSettle > 0))
    {
        // As with the focuser: the command is out but the controller has not reported motion yet.
        m_capSettle--;
        return;
    }

    CapParkSP.reset();

    if (arrived)
    {
        CapParkSP[(m_capTarget == CapTarget::Park) ? 0 : 1].setState(ISS_ON);
        CapParkSP.setState(IPS_OK);

        LOGF_INFO("Front flap %s.", (m_capTarget == CapTarget::Park) ? "closed" : "open");
    }
    else
    {
        CapParkSP.setState(IPS_ALERT);

        LOGF_ERROR("The front flap stopped at %d steps instead of %d. It may be obstructed, or its "
                   "calibration may be wrong.",
                   flap.position, target);
    }

    CapParkSP.apply();
    m_capTarget = CapTarget::None;
}

// ---------------------------------------------------------------------------------------------------
// Flat panel
// ---------------------------------------------------------------------------------------------------

void ScopeLink::publishLightBox(const scopelink::Status &status)
{
    const bool lit = status.flatboxDuty > 0;

    LightSP.reset();
    LightSP[lit ? 0 : 1].setState(ISS_ON);
    LightSP.setState(IPS_OK);
    LightSP.apply();

    // Only a lit panel's duty cycle says anything about the brightness the user asked for. Reading back a
    // duty of zero while the panel is off would wipe out the setting they typed before switching it on.
    if (lit)
    {
        m_lightIntensity = status.flatboxDuty;

        LightIntensityNP[0].setValue(m_lightIntensity);
        LightIntensityNP.setState(IPS_OK);
        LightIntensityNP.apply();
    }
}

// ---------------------------------------------------------------------------------------------------
// Client requests
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if ((dev == nullptr) || (strcmp(dev, getDeviceName()) != 0))
        return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);

    if (StepMultiplierNP.isNameMatch(name))
    {
        StepMultiplierNP.update(values, names, n);
        StepMultiplierNP.setState(IPS_OK);
        StepMultiplierNP.apply();

        // On a controller that holds the multiplier itself this property is a display of the
        // controller's value, and the place to change it is the identifier that holds it - where both
        // drivers will read the same number afterwards, which is the whole reason it moved into the
        // controller.
        if (commandsFunctions())
        {
            LOG_WARN("This controller holds the step multiplier itself. Set 'focuser.step_multiplier' on "
                     "the Parameters tab instead; what is typed here is not sent anywhere.");

            StepMultiplierNP[0].setValue(m_stepMultiplier);
            StepMultiplierNP.apply();

            return true;
        }

        if (isConnected())
            LOG_WARN("The step multiplier is applied when the driver connects; reconnect for it to take effect.");

        saveConfig(true, StepMultiplierNP.getName());

        return true;
    }

    if (!isReady())
        return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);

    if (LightIntensityNP.isNameMatch(name))
    {
        LightIntensityNP.update(values, names, n);

        m_lightIntensity = static_cast<int>(LightIntensityNP[0].getValue());

        try
        {
            // Writing the duty only while the panel is on keeps "off" meaning off. The stored value is
            // what the panel comes back to when it is switched on again.
            if (LightSP[0].getState() == ISS_ON)
                m_device->setFlatboxDuty(m_lightIntensity);

            LightIntensityNP.setState(IPS_OK);
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("The controller did not accept the flat panel duty cycle: %s", error.what());
            LightIntensityNP.setState(IPS_ALERT);
        }

        LightIntensityNP.apply();

        return true;
    }

    if (FanTargetNP.isNameMatch(name))
    {
        FanTargetNP.update(values, names, n);

        try
        {
            for (int index = 0; index < 2; index++)
                m_device->setFanTarget(index, FanTargetNP[index].getValue());

            m_device->refreshStatus();
            FanTargetNP.setState(IPS_OK);
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("The controller did not accept the fan target: %s", error.what());
            FanTargetNP.setState(IPS_ALERT);
        }

        FanTargetNP.apply();

        return true;
    }

    if (CalibrationStepNP.isNameMatch(name))
    {
        CalibrationStepNP.update(values, names, n);
        CalibrationStepNP.setState(IPS_OK);
        CalibrationStepNP.apply();
        saveConfig(true, CalibrationStepNP.getName());

        return true;
    }

    for (ParameterGroup &group : m_parameterGroups)
    {
        if ((group.property != nullptr) && group.property->isNameMatch(name))
            return applyParameterGroup(group, values, names, n);
    }

    if (strstr(name, "FOCUS_"))
        return FI::processNumber(dev, name, values, names, n);

    // Every property the rotator interface owns has ROTATOR in its name, which is what makes this the same
    // one line the focuser gets. It is last so that this driver's own properties - the sync offset among
    // them - are matched by name first.
    if (strstr(name, "ROTATOR"))
        return RI::processNumber(dev, name, values, names, n);

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool ScopeLink::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if ((dev == nullptr) || (strcmp(dev, getDeviceName()) != 0))
        return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    if (SimulatedGenerationSP.isNameMatch(name))
    {
        SimulatedGenerationSP.update(states, names, n);
        SimulatedGenerationSP.setState(IPS_OK);
        SimulatedGenerationSP.apply();

        if (isConnected())
            LOG_INFO("The simulated generation is read when the device connects, so this takes effect on "
                     "the next connection.");

        saveConfig(true, SimulatedGenerationSP.getName());

        return true;
    }

    if (DeveloperSP.isNameMatch(name))
    {
        DeveloperSP.update(states, names, n);
        DeveloperSP.setState(IPS_OK);
        DeveloperSP.apply();

        if (DeveloperSP[0].getState() == ISS_ON)
            defineDeveloperProperties();
        else
            deleteDeveloperProperties();

        saveConfig(true, DeveloperSP.getName());

        return true;
    }

    // Deliberately ahead of the isReady() gate below: a controller sitting in its boot loader is never
    // ready, and installing firmware is the only thing that will make it so.
    if (FirmwareActionSP.isNameMatch(name))
    {
        FirmwareActionSP.update(states, names, n);

        const int action = FirmwareActionSP.findOnSwitchIndex();

        if (action == 0)
        {
            std::unique_ptr<scopelink::FirmwareFile> file;

            FirmwareActionSP.reset();
            FirmwareActionSP.setState(checkFirmwareFile(file) ? IPS_OK : IPS_ALERT);
            FirmwareActionSP.apply();
        }
        else if (action == 1)
        {
            startFirmwareUpdate();

            if (!m_update)
            {
                FirmwareActionSP.reset();
                FirmwareActionSP.setState(IPS_ALERT);
                FirmwareActionSP.apply();
            }
        }

        return true;
    }

    if (!isReady())
        return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    if (CapParkSP.isNameMatch(name))
    {
        // The property is not published without a calibrated travel, but a client that names it directly
        // still reaches this, and there is no position to open to.
        if (!hasUsableFlap())
        {
            LOG_ERROR("The front flap has no calibrated travel, so it cannot be opened or closed.");
            CapParkSP.setState(IPS_ALERT);
            CapParkSP.apply();

            return true;
        }

        CapParkSP.update(states, names, n);

        const IPState result = moveCap((CapParkSP[0].getState() == ISS_ON) ? CapTarget::Park : CapTarget::Unpark);

        CapParkSP.setState(result);
        CapParkSP.apply();

        return true;
    }

    if (LightSP.isNameMatch(name))
    {
        LightSP.update(states, names, n);

        const bool on = LightSP[0].getState() == ISS_ON;

        try
        {
            m_device->setFlatboxDuty(on ? m_lightIntensity : 0);
            m_device->refreshStatus();
            LightSP.setState(IPS_OK);

            if (on && (m_lightIntensity == 0))
                LOG_WARN("The flat panel is on but its duty cycle is zero, so it will not light up.");
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("The controller did not accept the flat panel command: %s", error.what());
            LightSP.setState(IPS_ALERT);
        }

        LightSP.apply();

        return true;
    }

    if (FanOverrideSP.isNameMatch(name) || FanStateSP.isNameMatch(name))
    {
        const bool isOverride        = FanOverrideSP.isNameMatch(name);
        INDI::PropertySwitch &target = isOverride ? FanOverrideSP : FanStateSP;

        target.update(states, names, n);

        try
        {
            for (int index = 0; index < 2; index++)
            {
                const bool on = target[index].getState() == ISS_ON;

                if (isOverride)
                    m_device->setFanOverrideEnabled(index, on);
                else
                    m_device->setFanOverrideState(index, on);
            }

            m_device->refreshStatus();
            target.setState(IPS_OK);
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("The controller did not accept the fan command: %s", error.what());
            target.setState(IPS_ALERT);
        }

        target.apply();

        return true;
    }

    if (AuxPowerSP.isNameMatch(name))
    {
        AuxPowerSP.update(states, names, n);

        try
        {
            for (int index = 0; index < 2; index++)
                m_device->setPowerSwitch(index, AuxPowerSP[index].getState() == ISS_ON);

            m_device->refreshStatus();
            AuxPowerSP.setState(IPS_OK);
        }
        catch (const std::exception &error)
        {
            LOGF_ERROR("The controller did not accept the power command: %s", error.what());
            AuxPowerSP.setState(IPS_ALERT);
        }

        AuxPowerSP.apply();

        return true;
    }

    for (ParameterGroup &group : m_parameterGroups)
    {
        for (ParameterSwitch &entry : group.switches)
        {
            if (entry.property->isNameMatch(name))
                return applyParameterSwitch(entry, states, names, n);
        }
    }

    if (ParametersActionSP.isNameMatch(name))
    {
        ParametersActionSP.update(states, names, n);

        switch (ParametersActionSP.findOnSwitchIndex())
        {
            case 0:
                readAllParameters();
                break;

            case 1:
                exportParameters();
                break;

            case 2:
                importParameters();
                break;

            default:
                break;
        }

        ParametersActionSP.reset();
        ParametersActionSP.setState(IPS_OK);
        ParametersActionSP.apply();

        return true;
    }

    if (RestartSP.isNameMatch(name))
    {
        // Reset before the work rather than after it: a restart that goes as it should ends with the
        // device disconnected and connected again, and the switch the client sees then is a freshly
        // published one. Leaving it on here would light a button that has already done its job.
        RestartSP.update(states, names, n);
        RestartSP.reset();

        restartController();

        return true;
    }

    if (DiagnosticActionSP.isNameMatch(name))
    {
        DiagnosticActionSP.update(states, names, n);

        switch (DiagnosticActionSP.findOnSwitchIndex())
        {
            case 0:
                readFaults();
                break;

            case 1:
                clearFaults();
                break;

            case 2:
                readEepromStatistics();
                break;

            default:
                break;
        }

        DiagnosticActionSP.reset();
        DiagnosticActionSP.setState(IPS_OK);
        DiagnosticActionSP.apply();

        return true;
    }

    if (CalibrationMotorSP.isNameMatch(name))
    {
        CalibrationMotorSP.update(states, names, n);
        CalibrationMotorSP.setState(IPS_OK);
        CalibrationMotorSP.apply();

        return true;
    }

    if (CalibrationActionSP.isNameMatch(name))
    {
        CalibrationActionSP.update(states, names, n);

        const int steps = static_cast<int>(CalibrationStepNP[0].getValue());

        switch (CalibrationActionSP.findOnSwitchIndex())
        {
            case 0:
                calibrationJog(-steps);
                break;

            case 1:
                calibrationJog(steps);
                break;

            case 2:
                try
                {
                    m_device->haltMotor(selectedChannel().motorId);
                }
                catch (const std::exception &error)
                {
                    LOGF_ERROR("The controller did not accept the stop: %s", error.what());
                }
                break;

            case 3:
                calibrationMarkMinimum();
                break;

            case 4:
                calibrationMarkMaximum();
                break;

            case 5:
                calibrationReset();
                break;

            default:
                break;
        }

        CalibrationActionSP.reset();
        CalibrationActionSP.setState(IPS_OK);
        CalibrationActionSP.apply();

        return true;
    }

    if (strstr(name, "FOCUS_"))
        return FI::processSwitch(dev, name, states, names, n);

    if (strstr(name, "ROTATOR"))
        return RI::processSwitch(dev, name, states, names, n);

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeLink::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if ((dev == nullptr) || (strcmp(dev, getDeviceName()) != 0))
        return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);

    if (ParametersFileTP.isNameMatch(name))
    {
        ParametersFileTP.update(texts, names, n);
        ParametersFileTP.setState(IPS_OK);
        ParametersFileTP.apply();
        saveConfig(true, ParametersFileTP.getName());

        return true;
    }

    if (FirmwareFileTP.isNameMatch(name))
    {
        FirmwareFileTP.update(texts, names, n);
        FirmwareFileTP.setState(IPS_OK);
        FirmwareFileTP.apply();
        saveConfig(true, FirmwareFileTP.getName());

        return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool ScopeLink::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);
    RI::saveConfigItems(fp);

    RotatorOffsetNP.save(fp);
    StepMultiplierNP.save(fp);
    DeveloperSP.save(fp);
    SimulatedGenerationSP.save(fp);
    ParametersFileTP.save(fp);
    FirmwareFileTP.save(fp);
    CalibrationStepNP.save(fp);
    LightIntensityNP.save(fp);

    return true;
}
