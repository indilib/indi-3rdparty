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

    ChipNameTP[0].fill("NAME", "Name", "gpiochip0");
    ChipNameTP.fill(getDeviceName(), "CHIP_NAME", "Chip", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ChipNameTP.load();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
void INDIGPIO::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ChipNameTP);
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
            defineProperty(PWMFrequencyNP[i]);
            defineProperty(PWMDutyCycleNP[i]);
            defineProperty(PWMEnableSP[i]);
        }
    }
    else
    {
        // Delete PWM properties
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            deleteProperty(PWMFrequencyNP[i]);
            deleteProperty(PWMDutyCycleNP[i]);
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

        // Detect hardware PWM pins
        if (!detectHardwarePWM())
        {
            LOG_WARN("No hardware PWM pins detected");
        }
        else
        {
            LOGF_INFO("Detected %d hardware PWM pins", m_PWMPins.size());

            // Initialize PWM properties
            PWMFrequencyNP.clear();
            PWMDutyCycleNP.clear();
            PWMEnableSP.clear();

            // Reserve space for properties
            PWMFrequencyNP.reserve(m_PWMPins.size());
            PWMDutyCycleNP.reserve(m_PWMPins.size());
            PWMEnableSP.reserve(m_PWMPins.size());

            for (const auto &pin : m_PWMPins)
            {
                // Create properties
                auto name = "PWM" + std::to_string(pin.gpio);
                auto label = "PWM GPIO" + std::to_string(pin.gpio);

                // Frequency control
                INDI::PropertyNumber oneFrequency(1);
                oneFrequency[0].fill("FREQUENCY", "Frequency (Hz)", "%.0f", 1, 10000, 100, pin.frequency);
                oneFrequency.fill(getDeviceName(), (name + "_FREQUENCY").c_str(),
                                  (label + " Frequency").c_str(),
                                  "PWM", IP_RW, 60, IPS_IDLE);
                oneFrequency.load();
                PWMFrequencyNP.push_back(std::move(oneFrequency));

                // Duty cycle control
                INDI::PropertyNumber oneDutyCycle(1);
                oneDutyCycle[0].fill("DUTY_CYCLE", "Duty Cycle (%)", "%.1f", 0, 100, 10, pin.dutyCycle);
                oneDutyCycle.fill(getDeviceName(), (name + "_DUTY_CYCLE").c_str(),
                                  (label + " Duty Cycle").c_str(),
                                  "PWM", IP_RW, 60, IPS_IDLE);
                oneDutyCycle.load();
                PWMDutyCycleNP.push_back(std::move(oneDutyCycle));

                // Enable control
                INDI::PropertySwitch oneEnable(2);
                oneEnable[INDI_ENABLED].fill("INDI_ENABLED", "Enable", ISS_OFF);
                oneEnable[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
                oneEnable.fill(getDeviceName(), (name + "_ENABLE").c_str(),
                               (label + " Enable").c_str(),
                               "PWM", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
                PWMEnableSP.push_back(std::move(oneEnable));
            }

            // Optimize memory usage
            PWMFrequencyNP.shrink_to_fit();
            PWMDutyCycleNP.shrink_to_fit();
            PWMEnableSP.shrink_to_fit();
        }
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
    if (!m_DigitalLabelConfig)
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
    if (!m_AnalogLabelConfig)
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
            unexportPWMChannel(m_PWMPins[i].pwmChip, m_PWMPins[i].channel);
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
    INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);

    // Save PWM configurations
    for (size_t i = 0; i < m_PWMPins.size(); i++)
    {
        PWMFrequencyNP[i].save(fp);
        PWMDutyCycleNP[i].save(fp);
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
        // Handle PWM frequency changes
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            if (PWMFrequencyNP[i].isNameMatch(name))
            {
                PWMFrequencyNP[i].update(values, names, n);
                if (setPWMFrequency(i, static_cast<int>(values[0])))
                {
                    PWMFrequencyNP[i].setState(IPS_OK);
                    PWMFrequencyNP[i].apply();
                    saveConfig(PWMFrequencyNP[i]);
                }
                else
                {
                    PWMFrequencyNP[i].setState(IPS_ALERT);
                    PWMFrequencyNP[i].apply();
                }
                return true;
            }
        }

        // Handle PWM duty cycle changes
        for (size_t i = 0; i < m_PWMPins.size(); i++)
        {
            if (PWMDutyCycleNP[i].isNameMatch(name))
            {
                auto previousDuty = PWMDutyCycleNP[i][0].getValue();
                PWMDutyCycleNP[i].update(values, names, n);
                if (setPWMDutyCycle(i, static_cast<int>(values[0])))
                {
                    PWMDutyCycleNP[i].setState(IPS_OK);
                    PWMDutyCycleNP[i].apply();
                    if (previousDuty != values[0])
                        saveConfig(PWMDutyCycleNP[i]);
                }
                else
                {
                    PWMDutyCycleNP[i].setState(IPS_ALERT);
                    PWMDutyCycleNP[i].apply();
                }
                return true;
            }
        }
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
    std::ofstream periodFile(periodPath);
    if (!periodFile.is_open())
        return false;

    // Convert frequency to period in nanoseconds
    uint32_t period_ns = 1000000000 / frequency;
    periodFile << period_ns;
    periodFile.close();

    pin.frequency = frequency;
    return true;
}

bool INDIGPIO::setPWMDutyCycle(size_t index, int dutyCycle)
{
    if (index >= m_PWMPins.size() || dutyCycle < 0 || dutyCycle > 100)
        return false;

    auto &pin = m_PWMPins[index];
    std::string dutyPath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/duty_cycle";
    std::ofstream dutyFile(dutyPath);
    if (!dutyFile.is_open())
        return false;

    // Convert duty cycle percentage to nanoseconds
    uint32_t period_ns = 1000000000 / pin.frequency;
    uint32_t duty_ns = period_ns * dutyCycle / 100;
    dutyFile << duty_ns;
    dutyFile.close();

    pin.dutyCycle = dutyCycle;
    return true;
}

bool INDIGPIO::enablePWM(size_t index, bool enabled)
{
    if (index >= m_PWMPins.size())
        return false;

    auto &pin = m_PWMPins[index];

    // Export PWM channel if not already active
    if (enabled && !pin.active)
    {
        if (!exportPWMChannel(pin.pwmChip, pin.channel))
            return false;

        // Set period first
        if (!setPWMFrequency(index, pin.frequency))
            return false;

        // Then set duty cycle
        if (!setPWMDutyCycle(index, pin.dutyCycle))
            return false;
    }

    std::string enablePath = "/sys/class/pwm/" + pin.pwmChip + "/pwm" + std::to_string(pin.channel) + "/enable";
    std::ofstream enableFile(enablePath);
    if (!enableFile.is_open())
        return false;

    enableFile << (enabled ? "1" : "0");
    enableFile.close();

    pin.active = enabled;

    // Unexport if disabled
    if (!enabled)
        unexportPWMChannel(pin.pwmChip, pin.channel);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::detectHardwarePWM()
{
    const char *pwmPath = "/sys/class/pwm";
    DIR *dir = opendir(pwmPath);
    if (!dir)
    {
        LOGF_ERROR("Failed to open PWM directory: %s", pwmPath);
        return false;
    }

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

            // For each channel
            for (int channel = 0; channel < numChannels; channel++)
            {
                PWMPinConfig pin;
                pin.pwmChip = entry->d_name;
                pin.channel = channel;

                // Try to export the channel to get more info
                if (exportPWMChannel(pin.pwmChip, channel))
                {
                    // Read GPIO number if available
                    std::string gpioPath = chipPath + "/pwm" + std::to_string(channel) + "/gpio";
                    std::ifstream gpioFile(gpioPath);
                    if (gpioFile.is_open())
                    {
                        int gpio;
                        gpioFile >> gpio;
                        pin.gpio = gpio;
                        gpioFile.close();

                        // Add to our list of PWM pins
                        m_PWMPins.push_back(pin);

                        // Unexport for now, we'll export again when needed
                        unexportPWMChannel(pin.pwmChip, channel);
                    }
                }
            }
        }
    }
    closedir(dir);

    return !m_PWMPins.empty();
}

bool INDIGPIO::exportPWMChannel(const std::string &chip, uint8_t channel)
{
    std::string exportPath = "/sys/class/pwm/" + chip + "/export";
    std::ofstream exportFile(exportPath);
    if (!exportFile.is_open())
        return false;

    exportFile << channel;
    exportFile.close();

    // Give the system time to create the PWM files
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

bool INDIGPIO::unexportPWMChannel(const std::string &chip, uint8_t channel)
{
    std::string unexportPath = "/sys/class/pwm/" + chip + "/unexport";
    std::ofstream unexportFile(unexportPath);
    if (!unexportFile.is_open())
        return false;

    unexportFile << channel;
    unexportFile.close();
    return true;
}
