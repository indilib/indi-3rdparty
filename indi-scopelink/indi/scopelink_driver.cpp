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

const char *TELEMETRY_TAB   = "Telemetry";
const char *FANS_TAB        = "Fans";
const char *POWER_TAB       = "Power";
const char *PARAMETERS_TAB  = "Parameters";
const char *DIAGNOSTICS_TAB = "Diagnostics";
const char *CALIBRATION_TAB = "Calibration";

ScopeLink::ScopeLink() : INDI::FocuserInterface(this)
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

// ---------------------------------------------------------------------------------------------------
// Property construction
// ---------------------------------------------------------------------------------------------------

bool ScopeLink::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // The dust cap bit is added at connect time, because only a controller with the flap motor fitted has
    // a cover to park. Everything else is present on every generation.
    setDriverInterface(FOCUSER_INTERFACE | LIGHTBOX_INTERFACE | AUX_INTERFACE);

    FI::initProperties(FOCUS_TAB);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);

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

    // Which controller the simulated one pretends to be. Generations differ in what they offer, not only
    // in what they report, so this is the only way to see the USB hub and the smart switch diagnostics
    // appear and disappear without owning one controller of each generation.
    SimulatedGenerationSP[0].fill("GENERATION_2", "2", ISS_OFF);
    SimulatedGenerationSP[1].fill("GENERATION_3", "3", ISS_ON);
    SimulatedGenerationSP.fill(getDeviceName(), "SIMULATED_GENERATION", "Simulated hardware", OPTIONS_TAB, IP_RW,
                               ISR_1OFMANY, 60, IPS_IDLE);

    defineProperty(StepMultiplierNP);
    defineProperty(DeveloperSP);
    defineProperty(SimulatedGenerationSP);
    defineProperty(ParametersFileTP);

    loadConfig(true, StepMultiplierNP.getName());
    loadConfig(true, DeveloperSP.getName());
    loadConfig(true, SimulatedGenerationSP.getName());
    loadConfig(true, ParametersFileTP.getName());

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

    MotorLoadNP[0].fill("FOCUSER", "Focuser (%)", "%.0f", 0, 100, 1, 0);
    MotorLoadNP[1].fill("FLAP", "Flap (%)", "%.0f", 0, 100, 1, 0);
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

    UsbHubLP[0].fill("DS1_POWER", "Port 1 powered", IPS_IDLE);
    UsbHubLP[1].fill("DS2_POWER", "Port 2 powered", IPS_IDLE);
    UsbHubLP[2].fill("DS1_FAULT", "Port 1 fault", IPS_IDLE);
    UsbHubLP[3].fill("DS2_FAULT", "Port 2 fault", IPS_IDLE);
    UsbHubLP.fill(getDeviceName(), "USB_HUB_STATUS", "USB hub", POWER_TAB, IPS_IDLE);

    // --- Parameters, diagnostics and calibration ---------------------------------------------------------

    ParametersActionSP[0].fill("RELOAD", "Reload from controller", ISS_OFF);
    ParametersActionSP[1].fill("EXPORT", "Export to file", ISS_OFF);
    ParametersActionSP[2].fill("IMPORT", "Import from file", ISS_OFF);
    ParametersActionSP.fill(getDeviceName(), "PARAMETERS_ACTION", "Parameters", PARAMETERS_TAB, IP_RW, ISR_ATMOST1, 60,
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
    FI::updateProperties();

    if (isConnected())
    {
        defineProperty(IdentificationTP);
        defineHardwareProperties();

        if (DeveloperSP[0].getState() == ISS_ON)
            defineDeveloperProperties();

        // The travel just read from the controller is written back to the configuration file, so that a
        // client restoring an older one - after a recalibration, or after a session in simulation - is not
        // refused on the next start. It belongs here rather than beside the read because saveConfig only
        // has a property to write once the focuser interface has defined it.
        saveConfig(true, FocusMaxPosNP.getName());

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(IdentificationTP.getName());
        deleteHardwareProperties();
        deleteDeveloperProperties();
    }

    return true;
}

void ScopeLink::defineHardwareProperties()
{
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

    defineProperty(LightSP);
    defineProperty(LightIntensityNP);
    defineProperty(RailsNP);
    defineProperty(MotorLoadNP);
    defineProperty(FanOverrideSP);
    defineProperty(FanStateSP);

    defineProperty(AuxPowerSP);

    if (capabilities.hasUsbHub)
        defineProperty(UsbHubLP);

    for (ParameterGroup &group : m_parameterGroups)
        defineProperty(*group.property);

    defineProperty(ParametersActionSP);
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
    deleteProperty(FocusTemperatureNP.getName());
    deleteProperty(TemperatureNP.getName());
    deleteProperty(FanTargetNP.getName());
    deleteProperty(CapParkSP.getName());
    deleteProperty(CapPositionNP.getName());
    deleteProperty(LightSP.getName());
    deleteProperty(LightIntensityNP.getName());
    deleteProperty(RailsNP.getName());
    deleteProperty(MotorLoadNP.getName());
    deleteProperty(FanOverrideSP.getName());
    deleteProperty(FanStateSP.getName());
    deleteProperty(AuxPowerSP.getName());
    deleteProperty(UsbHubLP.getName());

    for (ParameterGroup &group : m_parameterGroups)
        deleteProperty(group.property->getName());

    m_parameterGroups.clear();

    deleteProperty(ParametersActionSP.getName());

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
    m_capTarget = CapTarget::None;
}

bool ScopeLink::identify()
{
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
        const int generation =
            SimulatedGenerationSP.findOnSwitchIndex() + scopelink::Capabilities::MinimumSupportedHardwareMajor;

        LOGF_INFO("Simulation is on. Answering as a generation %d controller; no port is opened.", generation);

        m_transport = std::unique_ptr<scopelink::ISerialTransport>(new scopelink::SimulatedTransport(generation));
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
        LOGF_ERROR("No ScopeLink answered on %s: %s", m_serialConnection->port(), error.what());
        return false;
    }

    const scopelink::Identification &identification = m_device->identification();
    const scopelink::Capabilities &capabilities     = m_device->capabilities();

    LOGF_INFO("Connected to ScopeLink %s", identification.toString().c_str());

    IdentificationTP[0].setText(std::to_string(identification.hardwareMajor) + "."
                                + std::to_string(identification.hardwareMinor));
    IdentificationTP[1].setText(std::to_string(identification.interfaceMajor) + "."
                                + std::to_string(identification.interfaceMinor));
    IdentificationTP[2].setText(identification.hardwareIdentifier);
    IdentificationTP[3].setText(identification.softwareIdentifier);
    IdentificationTP.setState(IPS_OK);

    if (!readMotorTravel(FocuserMaximumPositionDid, m_focuserTravel) || (m_focuserTravel <= 0))
    {
        LOG_ERROR("The focuser's calibrated travel could not be read from the controller.");
        return false;
    }

    // Reported rather than refused, unlike the flap. The driver never drives the focuser to its maximum on
    // its own, so an uncalibrated one only reaches an impossible position if a client asks for one, and
    // everywhere inside the physical travel still works. Refusing here would take a working focuser away
    // from anyone whose unit was never calibrated.
    if (m_focuserTravel == scopelink::motor::UncalibratedTravel)
    {
        LOG_WARN("The focuser has not been calibrated. The controller holds no end of travel for it, so the "
                 "maximum position below is the uncalibrated placeholder and nothing stops a client asking "
                 "for a position past the physical travel. Calibrate it on the Calibration tab.");
    }

    const int clientTravel = m_focuserTravel / m_stepMultiplier;

    FocusMaxPosNP[0].setValue(clientTravel);
    FocusMaxPosNP[0].setMinMax(0, clientTravel);
    FocusAbsPosNP[0].setMinMax(0, clientTravel);
    FocusRelPosNP[0].setMinMax(0, clientTravel);
    FocusSyncNP[0].setMinMax(0, clientTravel);

    LOGF_INFO("Focuser travel is %d controller steps, %d client steps at a multiplier of %d.", m_focuserTravel,
              clientTravel, m_stepMultiplier);

    m_flapTravel = 0;

    if (!readMotorTravel(FlapMaximumPositionDid, m_flapTravel) || (m_flapTravel <= 0))
    {
        LOG_ERROR("The front flap's calibrated travel could not be read from the controller.");
        return false;
    }

    // An uncalibrated flap is not offered as a cover at all. Unlike the focuser this does not stop the
    // connection, because everything else on the controller still works - but it must not be published
    // as a dust cap, because a dust cap is a thing Ekos opens and closes on its own schedule, and the
    // only move this one could make is towards a position two billion steps away.
    if (m_flapTravel == scopelink::motor::UncalibratedTravel)
    {
        LOG_WARN("The front flap has not been calibrated, so it is not offered as a dust cap. Calibrate "
                 "it on the Calibration tab, then reconnect.");
        m_flapTravel = 0;
    }
    else
    {
        CapPositionNP[0].setMinMax(0, m_flapTravel);
        LOGF_INFO("Front flap travel is %d controller steps.", m_flapTravel);
    }

    // Claimed only once each role has something working behind it: Ekos reads this bitmask to decide which
    // jobs it may give the device, so a role listed here is a role it will drive unattended.
    uint16_t interfaces = FOCUSER_INTERFACE | LIGHTBOX_INTERFACE | AUX_INTERFACE;

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

    // Where the focuser already is counts as where it was asked to be, so the first poll after connecting
    // does not report a stall against a target nobody set.
    m_focusTarget = static_cast<double>(m_device->status().motor1Position) / m_stepMultiplier;

    return true;
}

bool ScopeLink::Disconnect()
{
    // The descriptor belongs to libindi's connection plugin, which closes it as part of the call below.
    releaseDevice();

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
        m_serialConnection->Disconnect();

    setConnected(false, IPS_ALERT);
    updateProperties();
}

bool ScopeLink::readMotorTravel(uint32_t maximumPositionDid, int &travel)
{
    scopelink::Did identifier(maximumPositionDid, scopelink::DidType::UInt32, scopelink::DidGroup::Miscellaneous,
                              "Motor maximum position");

    if (!identifier.read(*m_device))
    {
        LOGF_ERROR("%s", identifier.lastError().c_str());
        return false;
    }

    travel = identifier.value();

    return true;
}

// ---------------------------------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------------------------------

void ScopeLink::TimerHit()
{
    if (!isConnected())
        return;

    try
    {
        const scopelink::Status &status = m_device->refreshStatus();

        m_failedPolls = 0;

        publishFocuser(status);
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
    const double position = static_cast<double>(status.motor1Position) / m_stepMultiplier;

    FocusAbsPosNP[0].setValue(position);

    // A stopped motor is not on its own proof that a move has finished: the controller has not
    // necessarily started by the time the poll after the command lands. Requiring the position as well
    // removes that race, and it turns a move that stopped short - the stall detection doing its job -
    // into an alert rather than a silently wrong "arrived".
    if ((FocusAbsPosNP.getState() == IPS_BUSY) && !status.motor1Moving)
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

void ScopeLink::publishTelemetry(const scopelink::Status &status)
{
    RailsNP[0].setValue(status.supplyVoltage / 1000.0);
    RailsNP[1].setValue(status.fanAVoltage / 1000.0);
    RailsNP[2].setValue(status.fanBVoltage / 1000.0);
    RailsNP.setState(IPS_OK);
    RailsNP.apply();

    MotorLoadNP[0].setValue(status.motor1Load);
    MotorLoadNP[1].setValue(status.motor2Load);
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
        UsbHubLP[0].setState(status.usb1PowerActive ? IPS_OK : IPS_IDLE);
        UsbHubLP[1].setState(status.usb2PowerActive ? IPS_OK : IPS_IDLE);
        UsbHubLP[2].setState(status.usb1PowerFailure ? IPS_ALERT : IPS_IDLE);
        UsbHubLP[3].setState(status.usb2PowerFailure ? IPS_ALERT : IPS_IDLE);
        UsbHubLP.setState((status.usb1PowerFailure || status.usb2PowerFailure) ? IPS_ALERT : IPS_OK);
        UsbHubLP.apply();
    }
}

// ---------------------------------------------------------------------------------------------------
// Focuser interface
// ---------------------------------------------------------------------------------------------------

IPState ScopeLink::MoveAbsFocuser(uint32_t targetTicks)
{
    if (!isReady())
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
        m_device->moveMotor(scopelink::motor::Focuser, static_cast<int>(native));
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
    if (!isReady())
        return false;

    try
    {
        m_device->haltMotor(scopelink::motor::Focuser);
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
    m_focusTarget = static_cast<double>(m_device->status().motor1Position) / m_stepMultiplier;

    LOG_INFO("Focuser motor halted.");

    return true;
}

bool ScopeLink::SyncFocuser(uint32_t ticks)
{
    if (!isReady())
        return false;

    try
    {
        m_device->syncMotor(scopelink::motor::Focuser, static_cast<int>(static_cast<long>(ticks) * m_stepMultiplier));
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
    if (!isReady() || (ticks == static_cast<uint32_t>(m_focuserTravel / m_stepMultiplier)))
        return true;

    // The travel limit is the controller's calibration, not a client preference. Letting the focus module
    // write it would let a stray number destroy a calibration that took a bench session to establish, and
    // the client has no way of knowing what the mechanics can actually reach.
    LOG_ERROR("The focuser travel comes from the controller's calibration and cannot be set from here. "
              "Use the Calibration tab to recalibrate it.");

    return false;
}

// ---------------------------------------------------------------------------------------------------
// Cover
// ---------------------------------------------------------------------------------------------------

IPState ScopeLink::moveCap(CapTarget target)
{
    const int position = (target == CapTarget::Park) ? 0 : m_flapTravel;

    try
    {
        // A flap that is already running is stopped first. Commanding a new target while the previous one
        // is still being followed is not something the controller is specified to accept.
        if (m_device->requireStatus().motor2Moving)
            m_device->haltMotor(scopelink::motor::Flap);

        m_device->moveMotor(scopelink::motor::Flap, position);
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

void ScopeLink::adoptCapStateFromPosition(const scopelink::Status &status)
{
    const int tolerance = std::max(MinimumCapTolerance, m_flapTravel / 200);

    CapParkSP.reset();

    if (std::abs(status.motor2Position) <= tolerance)
    {
        CapParkSP[0].setState(ISS_ON);
        CapParkSP.setState(IPS_OK);
    }
    else if (std::abs(status.motor2Position - m_flapTravel) <= tolerance)
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
                  status.motor2Position, m_flapTravel);
    }

    CapParkSP.apply();
}

void ScopeLink::publishCap(const scopelink::Status &status)
{
    if (!hasUsableFlap())
        return;

    CapPositionNP[0].setValue(status.motor2Position);
    CapPositionNP[1].setValue(
        (m_flapTravel > 0) ? std::min(1.0, std::max(0.0, static_cast<double>(status.motor2Position) / m_flapTravel)) :
                             0.0);
    CapPositionNP.setState(status.motor2Moving ? IPS_BUSY : IPS_OK);
    CapPositionNP.apply();

    if (m_capTarget == CapTarget::None)
    {
        // Nothing has been commanded since the driver connected, so the flap's state is whatever the
        // position says it is.
        if (CapParkSP.getState() == IPS_IDLE)
            adoptCapStateFromPosition(status);

        return;
    }

    if (status.motor2Moving)
        return;

    const int tolerance = std::max(MinimumCapTolerance, m_flapTravel / 200);
    const int target    = (m_capTarget == CapTarget::Park) ? 0 : m_flapTravel;
    const bool arrived  = std::abs(status.motor2Position - target) <= tolerance;

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
                   status.motor2Position, target);
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
        if (group.property->isNameMatch(name))
            return applyParameterGroup(group, values, names, n);
    }

    if (strstr(name, "FOCUS_"))
        return FI::processNumber(dev, name, values, names, n);

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

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeLink::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if ((dev != nullptr) && (strcmp(dev, getDeviceName()) == 0) && ParametersFileTP.isNameMatch(name))
    {
        ParametersFileTP.update(texts, names, n);
        ParametersFileTP.setState(IPS_OK);
        ParametersFileTP.apply();
        saveConfig(true, ParametersFileTP.getName());

        return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool ScopeLink::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);

    StepMultiplierNP.save(fp);
    DeveloperSP.save(fp);
    SimulatedGenerationSP.save(fp);
    ParametersFileTP.save(fp);
    CalibrationStepNP.save(fp);
    LightIntensityNP.save(fp);

    return true;
}
