/*******************************************************************************
  Copyright(c) 2024 Jasem Mutlaq <mutlaqja@ikarustech.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indi_gpio.h"
#include "config.h"

#include <dirent.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static class Loader
{
    public:
        std::unique_ptr<INDIGPIO> device;
    public:
        Loader()
        {
            device.reset(new INDIGPIO());
        }
} loader;

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
INDIGPIO::INDIGPIO() : INDI::InputInterface(this), INDI::OutputInterface(this)
{
    setVersion(VERSION_MAJOR, VERSION_MINOR);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
INDIGPIO::~INDIGPIO()
{
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
const char *INDIGPIO::getDefaultName()
{
    return "GPIO";
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | OUTPUT_INTERFACE | INPUT_INTERFACE);

    ChipNameTP[0].fill("NAME", "Name", "gpiochip0");
    ChipNameTP.fill(getDeviceName(), "CHIP_NAME", "Chip", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ChipNameTP.load();

    // Initialize PWM GPIO mapping
    PWMGPIOMappingNP.clear();

    // Scan for PWM chips
    const char *pwmPath = "/sys/class/pwm";
    DIR *dir = opendir(pwmPath);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            // Look for pwmchipX directories
            if (strncmp(entry->d_name, "pwmchip", 7) == 0)
            {
                std::string chipPath = std::string(pwmPath) + "/" + entry->d_name;
                std::string npwmPath = chipPath + "/npwm";

                // Read number of PWM channels
                std::ifstream npwmFile(npwmPath);
                if (!npwmFile.is_open())
                    continue;

                int numChannels;
                npwmFile >> numChannels;
                npwmFile.close();

                // Create mapping for this chip's channels
                INDI::PropertyNumber oneChipMapping {static_cast<size_t>(numChannels)};
                for (int i = 0; i < numChannels; i++)
                {
                    auto name = "CHANNEL" + std::to_string(i);
                    auto label = "Channel " + std::to_string(i) + " GPIO";
                    // Default GPIO12 to channel 0 of pwmchip0
                    auto defaultValue = (std::string(entry->d_name) == "pwmchip0" && i == 0) ? 12 : 0;
                    oneChipMapping[i].fill(name.c_str(), label.c_str(), "%.0f", 0, 40, 1, defaultValue);
                }
                oneChipMapping.fill(getDeviceName(), (std::string(entry->d_name) + "_MAP").c_str(),
                                    (std::string(entry->d_name) + " GPIO Mapping").c_str(),
                                    entry->d_name, IP_RW, 60, IPS_IDLE);
                oneChipMapping.load();
                PWMGPIOMappingNP.insert_or_assign(std::string(entry->d_name), std::move(oneChipMapping));
            }
        }
        closedir(dir);
    }

    addDebugControl();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
void INDIGPIO::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ChipNameTP);
    for (auto &[chip, mapping] : PWMGPIOMappingNP)
        defineProperty(mapping);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    INDI::InputInterface::updateProperties();
    INDI::OutputInterface::updateProperties();

    if (isConnected())
    {
        // Define PWM properties for each detected PWM pin
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            defineProperty(PWMConfigNP[i]);
            defineProperty(PWMEnableSP[i]);
        }
    }
    else
    {
        // Delete PWM properties
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            deleteProperty(PWMConfigNP[i]);
            deleteProperty(PWMEnableSP[i]);
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::Connect()
{
    try
    {
        m_GPIO.reset(new gpiod::chip(ChipNameTP[0].getText()));

        // Detect and setup PWM pins
        setupPWMProperties();
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to connect: %s", e.what());
        return false;
    }

    // Get all lines
    auto lines = m_GPIO->get_all_lines();

    m_InputOffsets.clear();
    m_OutputOffsets.clear();
    // Iterate through all lines
    for (const auto &line : lines)
    {
        // Get line direction
        auto name = line.name();

        // Skip lines that are used or not GPIO
        if (line.is_used() || name.find("GPIO") == std::string::npos)
            continue;

        // Skip GPIOs configured for PWM
        bool isPWM = std::any_of(m_PWMPins.begin(), m_PWMPins.end(),
                                 [&](const PWMPinConfig & pin)
        {
            return pin.gpio == line.offset();
        });
        if (isPWM)
            continue;

        auto direction = line.direction();

        // Check if line is input or output and add to corresponding vector
        if (direction == gpiod::line::DIRECTION_INPUT)
        {
            m_InputOffsets.push_back(line.offset());
        }
        else if (direction == gpiod::line::DIRECTION_OUTPUT)
        {
            m_OutputOffsets.push_back(line.offset());
        }

        line.release();
    }

    // Initialize Inputs. We do not support Analog inputs
    INDI::InputInterface::initProperties("Inputs", m_InputOffsets.size(), 0, "GPIO");
    // At this stage, all the labels and outputs are GPIO #1, GPIO #2 ..etc, but we
    // need to update the number to matches to actual offsets
    // We only do this if configuration is not loaded up
    if (!m_DigitalInputLabelsConfig)
    {
        for (size_t i = 0; i < m_InputOffsets.size(); i++)
        {
            auto label = "DI #" + std::to_string(i + 1) + " (GPIO " + std::to_string(m_InputOffsets[i]) + ")";
            DigitalInputLabelsTP[i].setText(label);
            DigitalInputsSP[i].setLabel(label);
        }
    }

    // Initialize outputs
    INDI::OutputInterface::initProperties("Outputs", m_OutputOffsets.size(), "GPIO");
    // If config not loaded, use default values
    if (!m_DigitalOutputLabelsConfig)
    {
        // At this stage, all the labels and outputs are GPIO #1, GPIO #2 ..etc, but we
        // need to update the number to matches to actual offsets
        for (size_t i = 0; i < m_OutputOffsets.size(); i++)
        {
            auto label = "DO #" + std::to_string(i + 1) + " (GPIO " + std::to_string(m_OutputOffsets[i]) + ")";
            DigitalOutputLabelsTP[i].setText(label);
            DigitalOutputsSP[i].setLabel(label);
        }
    }

    SetTimer(getPollingPeriod());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::Disconnect()
{
    // Cleanup PWM
    for (size_t i = 0; i < m_PWMPins.size(); i++)
    {
        if (m_PWMPins[i].active)
        {
            enablePWM(i, false);
        }
    }

    m_GPIO->reset();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    ChipNameTP.save(fp);
    for (auto &[chip, mapping] : PWMGPIOMappingNP)
        mapping.save(fp);
    INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);

    // Save PWM configurations
    for (size_t i = 0; i < m_PWMPins.size(); i++)
    {
        PWMConfigNP[i].save(fp);
        PWMEnableSP[i].save(fp);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::UpdateDigitalInputs()
{
    try
    {
        for (size_t i = 0; i < m_InputOffsets.size(); i++)
        {
            auto oldState = DigitalInputsSP[i].findOnSwitchIndex();
            auto line = m_GPIO->get_line(m_InputOffsets[i]);

            gpiod::line_request config;
            config.consumer = "indi-gpio";
            config.request_type = gpiod::line_request::DIRECTION_INPUT;
            line.request(config);
            auto newState = line.get_value();
            line.release();
            if (oldState != newState)
            {
                DigitalInputsSP[i].reset();
                DigitalInputsSP[i][newState].setState(ISS_ON);
                DigitalInputsSP[i].setState(IPS_OK);
                DigitalInputsSP[i].apply();
            }
        }
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to update digital inputs: %s", e.what());
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::UpdateAnalogInputs()
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::UpdateDigitalOutputs()
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::CommandOutput(uint32_t index, OutputState command)
{
    if (index >= m_OutputOffsets.size())
    {
        LOGF_ERROR("Invalid output index %d. Valid range from 0 to %d.", m_OutputOffsets.size() - 1);
        return false;
    }

    try
    {
        auto offset = m_OutputOffsets[index];
        auto line = m_GPIO->get_line(offset);
        gpiod::line_request config;
        config.consumer = "indi-gpio";
        config.request_type = gpiod::line_request::DIRECTION_OUTPUT;
        line.request(config);
        line.set_value(command);
        line.release();
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to toggle digital outputs: %s", e.what());
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
void INDIGPIO::TimerHit()
{
    if (!isConnected())
        return;

    UpdateDigitalInputs();
    UpdateDigitalOutputs();

    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Handle PWM enable switches
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            if (PWMEnableSP[i].isNameMatch(name))
            {
                auto wasEnabled = PWMEnableSP[i][0].getState() == ISS_ON;
                PWMEnableSP[i].update(states, names, n);
                bool enabled = PWMEnableSP[i][0].getState() == ISS_ON;
                if (enablePWM(i, enabled))
                {
                    PWMEnableSP[i].setState(IPS_OK);
                    PWMEnableSP[i].apply();
                    if (wasEnabled != enabled)
                        saveConfig(PWMEnableSP[i]);
                }
                else
                {
                    PWMEnableSP[i].setState(IPS_ALERT);
                    PWMEnableSP[i].apply();
                }
                return true;
            }
        }

        if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::InputInterface::processText(dev, name, texts, names, n))
            return true;
        if (INDI::OutputInterface::processText(dev, name, texts, names, n))
            return true;

        // Chip name
        if (ChipNameTP.isNameMatch(name))
        {
            ChipNameTP.update(texts, names, n);
            ChipNameTP.setState(IPS_OK);
            ChipNameTP.apply();
            saveConfig(ChipNameTP);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Handle PWM GPIO mapping
        for (auto &[chip, mapping] : PWMGPIOMappingNP)
        {
            if (mapping.isNameMatch(name))
            {
                if (mapping.isUpdated(values, names, n))
                {
                    mapping.update(values, names, n);
                    saveConfig(mapping);
                    LOG_INFO("PWM GPIO mapping updated. You must restart the system for this change to take effect.");
                }
                mapping.setState(IPS_OK);
                mapping.apply();
                return true;
            }
        }

        // Handle PWM configuration changes
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            if (PWMConfigNP[i].isNameMatch(name))
            {
                PWMConfigNP[i].update(values, names, n);
                bool success = true;

                // Get new values
                int newFrequency = static_cast<int>(PWMConfigNP[i][FREQUENCY].getValue());
                int newDutyCycle = static_cast<int>(PWMConfigNP[i][DUTY_CYCLE].getValue());

                // Check if PWM is enabled first
                if (!m_PWMPins[i].active)
                {
                    // Just store the values for when PWM is enabled
                    m_PWMPins[i].frequency = newFrequency;
                    m_PWMPins[i].dutyCycle = newDutyCycle;
                    LOGF_DEBUG("Stored PWM%d (GPIO%d) settings: frequency=%d Hz, duty cycle=%d%%",
                               i, m_PWMPins[i].gpio, newFrequency, newDutyCycle);
                    success = true;
                }
                else
                {
                    LOGF_INFO("Setting PWM%d (GPIO%d) frequency to %d Hz, duty cycle to %d%%",
                              i, m_PWMPins[i].gpio, newFrequency, newDutyCycle);

                    // Update frequency if changed
                    if (setPWMFrequency(i, newFrequency))
                    {
                        // Update duty cycle if frequency was set successfully
                        if (!setPWMDutyCycle(i, newDutyCycle))
                        {
                            LOGF_ERROR("Failed to set PWM%d duty cycle to %d%%", i, newDutyCycle);
                            success = false;
                        }
                    }
                    else
                    {
                        LOGF_ERROR("Failed to set PWM%d frequency to %d Hz", i, newFrequency);
                        success = false;
                    }
                }

                if (success)
                {
                    PWMConfigNP[i].setState(IPS_OK);
                    PWMConfigNP[i].apply();
                    saveConfig(PWMConfigNP[i]);
                }
                else
                {
                    PWMConfigNP[i].setState(IPS_ALERT);
                    PWMConfigNP[i].apply();
                }
                return true;
            }
        }

        if (INDI::OutputInterface::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::setPWMFrequency(size_t index, int frequency)
{
    if (index >= m_PWMPins.size() || frequency <= 0)
        return false;

    auto &pin = m_PWMPins[index];
    std::string periodPath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/period";

    // Verify file is writable before attempting to write
    if (access(periodPath.c_str(), W_OK) == -1)
    {
        LOGF_ERROR("No write permission for period file: %s", periodPath.c_str());
        return false;
    }

    // Convert frequency to period in nanoseconds
    uint32_t period_ns = 1000000000 / frequency;

    // Read current period first
    std::ifstream readFile(periodPath);
    if (readFile.is_open())
    {
        uint32_t current_period;
        readFile >> current_period;
        readFile.close();

        // Skip write if period is already set correctly
        if (current_period == period_ns)
        {
            pin.frequency = frequency;
            return true;
        }
    }

    // Disable PWM before changing period
    std::string enablePath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/enable";
    std::ofstream enableFile(enablePath);
    if (enableFile.is_open())
    {
        enableFile << "0" << std::endl << std::flush;
        enableFile.close();

        // Give hardware time to disable
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check if period is within valid range (1us to 1s)
    if (period_ns < 1000 || period_ns > 1000000000)
    {
        LOGF_ERROR("Invalid period value %d ns. Must be between 1000 ns and 1000000000 ns", period_ns);
        return false;
    }

    // Write new period value
    std::ofstream periodFile(periodPath);
    if (!periodFile.is_open())
    {
        LOGF_ERROR("Failed to open period file: %s", periodPath.c_str());
        return false;
    }

    // Write period value
    periodFile << period_ns << std::endl << std::flush;
    if (periodFile.fail())
    {
        LOGF_ERROR("Failed to write period value %d to %s. Check PWM hardware configuration and permissions.",
                   period_ns, periodPath.c_str());
        periodFile.close();
        return false;
    }
    periodFile.close();

    // Give hardware time to apply new period
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify the write
    std::ifstream verifyFile(periodPath);
    if (verifyFile.is_open())
    {
        uint32_t readValue;
        verifyFile >> readValue;
        if (readValue != period_ns)
        {
            LOGF_ERROR("Period verification failed. Wrote %d but read %d. Try setting a lower frequency.", period_ns, readValue);
            return false;
        }
        verifyFile.close();
    }

    pin.frequency = frequency;
    return true;
}

bool INDIGPIO::setPWMDutyCycle(size_t index, int dutyCycle)
{
    if (index >= m_PWMPins.size() || dutyCycle < 0 || dutyCycle > 100)
        return false;

    auto &pin = m_PWMPins[index];
    std::string dutyPath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/duty_cycle";

    // Verify file is writable before attempting to write
    if (access(dutyPath.c_str(), W_OK) == -1)
    {
        LOGF_ERROR("No write permission for duty cycle file: %s", dutyPath.c_str());
        return false;
    }

    // Convert duty cycle percentage to nanoseconds
    uint64_t period_ns = 1000000000ULL / pin.frequency;
    uint64_t duty_ns = (period_ns * dutyCycle) / 100;

    // Read current duty cycle first
    std::ifstream readFile(dutyPath);
    if (readFile.is_open())
    {
        uint32_t current_duty;
        readFile >> current_duty;
        readFile.close();

        // Skip write if duty cycle is already set correctly
        if (current_duty == duty_ns)
        {
            pin.dutyCycle = dutyCycle;
            return true;
        }
    }

    // Write new duty cycle value
    std::ofstream dutyFile(dutyPath);
    if (!dutyFile.is_open())
    {
        LOGF_ERROR("Failed to open duty cycle file: %s", dutyPath.c_str());
        return false;
    }

    dutyFile << duty_ns << std::endl << std::flush;
    if (dutyFile.fail())
    {
        LOGF_ERROR("Failed to write duty cycle value %d to %s", duty_ns, dutyPath.c_str());
        dutyFile.close();
        return false;
    }
    dutyFile.close();

    // Verify the write
    std::ifstream verifyFile(dutyPath);
    if (verifyFile.is_open())
    {
        uint32_t readValue;
        verifyFile >> readValue;
        if (readValue != duty_ns)
        {
            LOGF_ERROR("Duty cycle verification failed. Wrote %llu but read %d", (unsigned long long)duty_ns, readValue);
            return false;
        }
        verifyFile.close();
    }

    pin.dutyCycle = dutyCycle;
    return true;
}

bool INDIGPIO::enablePWM(size_t index, bool enabled)
{
    if (index >= m_PWMPins.size())
        return false;

    auto &pin = m_PWMPins[index];

    // Only enable if not already active
    if (enabled && !pin.active)
    {
        // Set period first
        if (!setPWMFrequency(index, pin.frequency))
        {
            LOGF_ERROR("Failed to set PWM frequency to %d Hz", pin.frequency);
            return false;
        }

        // Give hardware time to stabilize after period change
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Then set duty cycle
        if (!setPWMDutyCycle(index, pin.dutyCycle))
        {
            LOGF_ERROR("Failed to set PWM duty cycle to %d%%", pin.dutyCycle);
            return false;
        }
    }

    std::string enablePath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/enable";

    // Verify file is writable before attempting to write
    if (access(enablePath.c_str(), W_OK) == -1)
    {
        LOGF_ERROR("No write permission for enable file: %s", enablePath.c_str());
        return false;
    }

    std::ofstream enableFile(enablePath);
    if (!enableFile.is_open())
    {
        LOGF_ERROR("Failed to open enable file: %s", enablePath.c_str());
        return false;
    }

    enableFile << (enabled ? "1" : "0") << "\n" << std::flush;
    if (enableFile.fail())
    {
        LOGF_ERROR("Failed to write enable value %d to %s", enabled ? 1 : 0, enablePath.c_str());
        enableFile.close();
        return false;
    }
    enableFile.close();

    // Verify the write
    std::ifstream verifyFile(enablePath);
    if (verifyFile.is_open())
    {
        int readValue;
        verifyFile >> readValue;
        if (readValue != (enabled ? 1 : 0))
        {
            LOGF_ERROR("Enable verification failed. Wrote %d but read %d", enabled ? 1 : 0, readValue);
            return false;
        }
        verifyFile.close();
    }

    pin.active = enabled;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
void INDIGPIO::setupPWMProperties()
{
    // Detect new PWM pins
    if (!detectHardwarePWM())
    {
        LOG_WARN("No hardware PWM pins detected");
        return;
    }

    LOGF_INFO("Detected %d hardware PWM pins", m_PWMPins.size());

    // Initialize PWM properties
    PWMConfigNP.clear();
    PWMEnableSP.clear();

    // Reserve space for properties
    PWMConfigNP.reserve(m_PWMPins.size());
    PWMEnableSP.reserve(m_PWMPins.size());

    for (const auto &pin : m_PWMPins)
    {
        // Create properties
        auto name = "PWM" + std::to_string(pin.gpio);
        auto label = "PWM GPIO" + std::to_string(pin.gpio);
        auto tab = pin.pwmChip;

        // PWM Configuration (Frequency and Duty Cycle)
        INDI::PropertyNumber oneConfig {N_PWM_CONFIG};
        // Default to 1000Hz to match initial export frequency
        oneConfig[FREQUENCY].fill("FREQUENCY", "Frequency (Hz)", "%.0f", 1, 10000, 100, pin.frequency > 0 ? pin.frequency : 1000);
        oneConfig[DUTY_CYCLE].fill("DUTY_CYCLE", "Duty Cycle (%)", "%.1f", 0, 100, 10, pin.dutyCycle);
        oneConfig.fill(getDeviceName(), (name + "_CONFIG").c_str(),
                       label.c_str(),
                       tab.c_str(), IP_RW, 60, IPS_IDLE);
        oneConfig.load();
        PWMConfigNP.push_back(std::move(oneConfig));

        // Enable control
        INDI::PropertySwitch oneEnable {2};
        oneEnable[INDI_ENABLED].fill("INDI_ENABLED", "Enable", pin.active ? ISS_ON : ISS_OFF);
        oneEnable[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", pin.active ? ISS_OFF : ISS_ON);
        oneEnable.fill(getDeviceName(), (name + "_ENABLE").c_str(),
                       (label + " Enable").c_str(),
                       tab.c_str(), IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        PWMEnableSP.push_back(std::move(oneEnable));
    }

    // Optimize memory usage
    PWMConfigNP.shrink_to_fit();
    PWMEnableSP.shrink_to_fit();
}

bool INDIGPIO::detectHardwarePWM()
{
    m_PWMPins.clear();
    const char *pwmPath = "/sys/class/pwm";
    DIR *dir = opendir(pwmPath);
    if (!dir)
    {
        LOGF_ERROR("Failed to open PWM directory: %s", pwmPath);
        return false;
    }

    LOG_INFO("Scanning for PWM chips in /sys/class/pwm...");
    LOGF_DEBUG("PWM directory exists: %s", access(pwmPath, F_OK) != -1 ? "Yes" : "No");
    LOGF_DEBUG("PWM directory readable: %s", access(pwmPath, R_OK) != -1 ? "Yes" : "No");
    LOGF_DEBUG("PWM directory writable: %s", access(pwmPath, W_OK) != -1 ? "Yes" : "No");

    // Check if PWM directory exists and is accessible
    if (access(pwmPath, R_OK) == -1)
    {
        LOGF_ERROR("Cannot access PWM directory: %s. PWM subsystem may not be enabled.", pwmPath);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Look for pwmchipX directories
        if (strncmp(entry->d_name, "pwmchip", 7) == 0)
        {
            LOGF_INFO("Found PWM chip: %s", entry->d_name);

            // Check permissions on PWM chip directory
            std::string chipDir = std::string(pwmPath) + "/" + entry->d_name;
            if (access(chipDir.c_str(), W_OK) == -1)
            {
                LOGF_ERROR("No write permission for PWM chip directory: %s. Try running with sudo or adding udev rules for pwm group.",
                           chipDir.c_str());
                continue;
            }

            LOGF_INFO("PWM chip %s has write access, reading number of channels...", entry->d_name);
            std::string chipPath = std::string(pwmPath) + "/" + entry->d_name;
            std::string npwmPath = chipPath + "/npwm";

            // Read number of PWM channels
            std::ifstream npwmFile(npwmPath);
            if (!npwmFile.is_open())
                continue;

            int numChannels;
            npwmFile >> numChannels;
            npwmFile.close();
            LOGF_INFO("PWM chip %s has %d channels", entry->d_name, numChannels);

            // Check which channels are already exported
            for (int i = 0; i < numChannels; i++)
            {
                std::string pwmPath = chipPath + "/pwm" + std::to_string(i);
                if (access(pwmPath.c_str(), F_OK) != -1)
                {
                    LOGF_INFO("PWM channel %d on %s is already exported at %s", i, entry->d_name, pwmPath.c_str());
                }
                else
                {
                    LOGF_INFO("PWM channel %d on %s is not exported", i, entry->d_name);
                }
            }

            // For each channel
            for (int channel = 0; channel < numChannels; channel++)
            {
                PWMPinConfig pin;
                pin.pwmChip = entry->d_name;
                pin.channel = channel;

                // Get GPIO mapping first
                auto it = PWMGPIOMappingNP.find(pin.pwmChip);
                if (it == PWMGPIOMappingNP.end())
                {
                    LOGF_DEBUG("No GPIO mapping found for PWM chip %s", pin.pwmChip.c_str());
                    continue;
                }
                auto gpioMapping = it->second[channel].getValue();
                LOGF_INFO("PWM channel %d on %s is mapped to GPIO%.0f", channel, entry->d_name, gpioMapping);

                // Skip if no mapping provided
                if (gpioMapping == 0)
                {
                    LOGF_DEBUG("Skipping PWM channel %d on %s: no GPIO mapping provided", channel, entry->d_name);
                    continue;
                }

                // Get GPIO number from mapping
                auto gpio = static_cast<int>(gpioMapping);
                if (gpio < 0)
                {
                    LOGF_WARN("Invalid GPIO number for PWM channel %d on %s: %d", channel, entry->d_name, gpio);
                    continue;
                }

                pin.gpio = gpio;

                // Export the channel if not already exported
                std::string pwmPath = chipPath + "/pwm" + std::to_string(channel);
                if (access(pwmPath.c_str(), F_OK) == -1)
                {
                    // Export the channel by writing to export file
                    std::string exportPath = chipPath + "/export";
                    LOGF_INFO("Exporting PWM channel %d by writing to %s", channel, exportPath.c_str());

                    std::ofstream exportFile(exportPath);
                    if (!exportFile.is_open())
                    {
                        LOGF_ERROR("Failed to open export file: %s", exportPath.c_str());
                        continue;
                    }

                    exportFile << channel;
                    exportFile.close();

                    // Give the system time to create the PWM files
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    // Verify the PWM directory was created and is writable
                    if (access(pwmPath.c_str(), F_OK) == -1)
                    {
                        LOGF_ERROR("Failed to export PWM channel - directory not created: %s", pwmPath.c_str());
                        continue;
                    }

                    // Check if we can write to the PWM files
                    std::string periodPath = pwmPath + "/period";
                    std::string dutyPath = pwmPath + "/duty_cycle";
                    std::string enablePath = pwmPath + "/enable";

                    if (access(periodPath.c_str(), W_OK) == -1 ||
                            access(dutyPath.c_str(), W_OK) == -1 ||
                            access(enablePath.c_str(), W_OK) == -1)
                    {
                        LOGF_ERROR("No write permission for PWM files in %s. Try running with sudo or adding udev rules.", pwmPath.c_str());
                        continue;
                    }

                    // Set initial period and duty cycle to avoid glitches
                    std::ofstream periodFile(periodPath);
                    if (periodFile.is_open())
                    {
                        // Default to 1000Hz (1ms period)
                        periodFile << "1000000" << std::endl << std::flush;  // 1ms in nanoseconds
                        periodFile.close();
                    }

                    std::ofstream dutyFile(dutyPath);
                    if (dutyFile.is_open())
                    {
                        // Default to 0% duty cycle
                        dutyFile << "0" << std::endl << std::flush;
                        dutyFile.close();
                    }

                    // Verify writes
                    std::ifstream verifyPeriod(periodPath);
                    if (verifyPeriod.is_open())
                    {
                        uint32_t period;
                        verifyPeriod >> period;
                        verifyPeriod.close();
                        if (period != 1000000)
                        {
                            LOGF_ERROR("Failed to set initial period. Expected 1000000, got %d", period);
                            continue;
                        }
                    }

                    // Set polarity to inversed and verify
                    std::string polarityPath = pwmPath + "/polarity";
                    std::ofstream polarityFile(polarityPath);
                    if (polarityFile.is_open())
                    {
                        polarityFile << "inversed" << std::endl << std::flush;
                        polarityFile.close();
                    }

                    // Set initial enable state to 0 and verify
                    std::ofstream enableFile(enablePath);
                    if (enableFile.is_open())
                    {
                        enableFile << "0" << std::endl << std::flush;
                        enableFile.close();

                        // Verify enable state
                        std::ifstream verifyEnable(enablePath);
                        if (verifyEnable.is_open())
                        {
                            int enabled;
                            verifyEnable >> enabled;
                            verifyEnable.close();
                            if (enabled != 0)
                            {
                                LOGF_ERROR("Failed to set initial enable state. Expected 0, got %d", enabled);
                                continue;
                            }
                        }
                    }

                    LOG_INFO("Successfully exported PWM channel with write access and default settings");
                }

                // Read current PWM settings
                std::string basePath = pwmPath;

                // Check if PWM files exist and are readable
                std::string enablePath = basePath + "/enable";
                std::string periodPath = basePath + "/period";
                std::string dutyPath = basePath + "/duty_cycle";

                if (access(enablePath.c_str(), R_OK) == -1 ||
                        access(periodPath.c_str(), R_OK) == -1 ||
                        access(dutyPath.c_str(), R_OK) == -1)
                {
                    LOGF_ERROR("Cannot read PWM files in %s", basePath.c_str());
                    continue;
                }

                // Read current settings
                std::ifstream enableFile(enablePath);
                if (enableFile.is_open())
                {
                    int enabled;
                    enableFile >> enabled;
                    pin.active = (enabled == 1);
                    enableFile.close();

                    if (pin.active)
                    {
                        // Read period (for frequency)
                        std::ifstream periodFile(basePath + "/period");
                        if (periodFile.is_open())
                        {
                            uint32_t period_ns;
                            periodFile >> period_ns;
                            pin.frequency = 1000000000 / period_ns;
                            periodFile.close();
                        }

                        // Read duty cycle
                        std::ifstream dutyFile(basePath + "/duty_cycle");
                        if (dutyFile.is_open())
                        {
                            uint32_t duty_ns;
                            dutyFile >> duty_ns;
                            pin.dutyCycle = (duty_ns * 100) / (1000000000 / pin.frequency);
                            dutyFile.close();
                        }
                    }
                }

                m_PWMPins.push_back(pin);
                LOGF_INFO("Using PWM channel %d on %s for GPIO%d (Active: %s)",
                          channel, pin.pwmChip.c_str(), pin.gpio, pin.active ? "Yes" : "No");
            }
        }
    }
    closedir(dir);

    return !m_PWMPins.empty();
}

bool INDIGPIO::exportPWMChannel(const std::string &chip, uint8_t channel)
{
    // Check if channel is already exported
    std::string pwmPath = "/sys/class/pwm/" + chip + "/pwm" + std::to_string(channel);
    DIR *pwmDir = opendir(pwmPath.c_str());
    if (pwmDir)
    {
        closedir(pwmDir);
        return true;  // Channel already exists and is accessible
    }

    // Check permissions on PWM chip directory
    std::string chipDir = "/sys/class/pwm/" + chip;
    if (access(chipDir.c_str(), W_OK) == -1)
    {
        LOGF_ERROR("No write permission for PWM chip directory: %s. Try running as root or adding udev rules.", chipDir.c_str());
        return false;
    }

    // Export the channel if it doesn't exist
    std::string exportPath = chipDir + "/export";
    LOGF_INFO("Exporting PWM channel %d by writing to %s", channel, exportPath.c_str());

    std::ofstream exportFile(exportPath);
    if (!exportFile.is_open())
    {
        LOGF_ERROR("Failed to open export file: %s. Error: %s", exportPath.c_str(), strerror(errno));
        return false;
    }

    exportFile << channel;
    exportFile.close();
    LOGF_DEBUG("Wrote channel number %d to export file", channel);

    // Give the system time to create the PWM files
    LOGF_DEBUG("Waiting for PWM files to be created at %s", pwmPath.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify the PWM files were created with proper permissions
    if (access(pwmPath.c_str(), W_OK) == -1)
    {
        LOGF_ERROR("No write permission for PWM channel directory: %s. Try running as root or adding udev rules.", pwmPath.c_str());
        return false;
    }

    LOGF_DEBUG("Successfully exported PWM channel %d on %s", channel, chip.c_str());
    return true;
}

bool INDIGPIO::unexportPWMChannel(const std::string &chip, uint8_t channel)
{
    std::string unexportPath = "/sys/class/pwm/" + chip + "/unexport";
    std::ofstream unexportFile(unexportPath);
    if (!unexportFile.is_open())
    {
        LOGF_ERROR("Failed to open unexport file: %s", unexportPath.c_str());
        return false;
    }

    unexportFile << channel;
    unexportFile.close();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
int INDIGPIO::getTotalPWMChannels()
{
    int totalChannels = 0;
    const char *pwmPath = "/sys/class/pwm";
    DIR *dir = opendir(pwmPath);
    if (!dir)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Look for pwmchipX directories
        if (strncmp(entry->d_name, "pwmchip", 7) == 0)
        {
            std::string chipPath = std::string(pwmPath) + "/" + entry->d_name;
            std::string npwmPath = chipPath + "/npwm";

            // Read number of PWM channels
            std::ifstream npwmFile(npwmPath);
            if (!npwmFile.is_open())
                continue;

            int numChannels;
            npwmFile >> numChannels;
            npwmFile.close();

            totalChannels += numChannels;
        }
    }
    closedir(dir);

    return totalChannels;
}
