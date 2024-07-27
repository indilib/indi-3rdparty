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

#pragma once

#include <defaultdevice.h>
#include <indioutputinterface.h>
#include <indiinputinterface.h>

#include <gpiod.hpp>

class INDIGPIO : public INDI::DefaultDevice, public INDI::InputInterface, public INDI::OutputInterface
{
    public:
        INDIGPIO();
        virtual ~INDIGPIO();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev);

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;

    protected:

        /**
             * \brief Update all digital inputs
             * \return True if operation is successful, false otherwise
             */
        virtual bool UpdateDigitalInputs() override;

        /**
         * \brief Update all analog inputs
         * \return True if operation is successful, false otherwise
         */
        virtual bool UpdateAnalogInputs() override;

        /**
         * \brief Update all digital outputs
         * \return True if operation is successful, false otherwise
         * \note UpdateDigitalOutputs should either be called periodically in the child's TimerHit or custom timer function or when an
         * interrupt or trigger warrants updating the digital outputs. Only updated properties that had a change in status since the last
         * time this function was called should be sent to the clients to reduce unnecessary updates.
         * Polling or Event driven implemetation depends on the concrete class hardware capabilities.
         */
        virtual bool UpdateDigitalOutputs() override;

        /**
         * \brief Send command to output
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandOutput(uint32_t index, OutputState command) override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual void TimerHit() override;

    private:
        INDI::PropertyText ChipNameTP {1};
        std::unique_ptr<gpiod::chip> m_GPIO;
        std::vector<uint8_t> m_InputOffsets, m_OutputOffsets;
};
