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
    setVersion(0, 1);

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

    if (isConnected())
    {

    }
    else
    {

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

    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to connect: %s", e.what());
        return false;
    }


    // Get all lines
    // Get a list of all lines
    auto lines = m_GPIO->get_all_lines();

    // Create vectors to store input and output lines


    m_InputOffsets.clear();
    m_OutputOffsets.clear();
    // Iterate through all lines
    for (const auto &line : lines)
    {
        // Get line direction
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
    }

    // Initialize outputs
    INDI::OutputInterface::initProperties("Outputs", m_OutputOffsets.size(), "GPIO");
    // At this stage, all the labels and outputs are GPIO #1, GPIO #2 ..etc, but we
    // need to update the number to matches to actual offsets
    for (size_t i = 0; i < m_OutputOffsets.size(); i++)
    {
        auto label = "GPIO #" + std::to_string(m_OutputOffsets[i]);
        DigitalOutputLabelsTP[i].setText(label);
        DigitalOutputsSP[i].setLabel(label);
    }

    // Initialize Inputs. We do not support Analog inputs
    INDI::InputInterface::initProperties("Inputs", m_InputOffsets.size(), 0, "GPIO");
    // At this stage, all the labels and outputs are GPIO #1, GPIO #2 ..etc, but we
    // need to update the number to matches to actual offsets
    for (size_t i = 0; i < m_InputOffsets.size(); i++)
    {
        auto label = "GPIO #" + std::to_string(m_InputOffsets[i]);
        DigitalInputLabelsTP[i].setText(label);
        DigitalInputsSP[i].setLabel(label);
    }

    return true;
}



////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool INDIGPIO::Disconnect()
{
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
            auto newState = line.get_value();
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
    try
    {
        for (size_t i = 0; i < m_OutputOffsets.size(); i++)
        {
            auto oldState = DigitalOutputsSP[i].findOnSwitchIndex();
            auto line = m_GPIO->get_line(m_OutputOffsets[i]);
            auto newState = line.get_value();
            if (oldState != newState)
            {
                DigitalOutputsSP[i].reset();
                DigitalOutputsSP[i][newState].setState(ISS_ON);
                DigitalOutputsSP[i].setState(IPS_OK);
                DigitalOutputsSP[i].apply();
            }
        }
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to read digital outputs: %s", e.what());
        return false;
    }
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
        line.set_value(command);
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
