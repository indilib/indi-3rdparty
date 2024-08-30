/*******************************************************************************
  Copyright(c) 2024 Colin McGill and Rodolphe Pineau. All rights reserved.

  AStarBox power control driver.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"

#include <vector>
#include <stdint.h>
#include "AStarBox.h"


class AStarBox : public INDI::DefaultDevice
{
    public:
        AStarBox();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
  
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override; 

    protected:
        const char *getDefaultName() override;
        bool Connect();
        bool Disconnect();
        virtual bool saveConfigItems(FILE *fp) override;

        // Event loop
        virtual void TimerHit() override;
 
private:
        CAStarBoxPowerPorts m_AStarBoxPort; // Interface to AStarBox ports
        
        bool setupComplete { false };
        // Functions to set, get and save data
        bool setPowerPorts();
        int saveBootValues();
        bool getSensorData();


        /////////////////////////////////////////////////////////////////////
        /// Indi Controls
        /////////////////////////////////////////////////////////////////////
  
        /////////////////////////////////////////////////////////////////////
        /// Power Group
        /////////////////////////////////////////////////////////////////////

        // PowerPorts
        INDI::PropertySwitch PowerPortsSP {4};
        enum
        {
          POWER_PORT_1,
          POWER_PORT_2,
          POWER_PORT_3,
          POWER_PORT_4
        };

        /////////////////////////////////////////////////////////////////////
        /// Dew Group
        /////////////////////////////////////////////////////////////////////

        // Dew PWM
        INDI::PropertyNumber DewPWMNP {2};
        enum
        {
            DEW_PWM_1,
            DEW_PWM_2,
        };

        /////////////////////////////////////////////////////////////////////
        /// Sensor Group
        /////////////////////////////////////////////////////////////////////

        // Power Sensors
        INDI::PropertyNumber PowerSensorsNP {1};
        enum
        {
            SENSOR_VOLTAGE,
        };

  
};
