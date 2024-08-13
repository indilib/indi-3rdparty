/*
    Toupcam & oem Focuser
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include <indifocuser.h>
#include <indipropertytext.h>
#include "libtoupbase.h"

class ToupAAF : public INDI::Focuser
{
    public:
        ToupAAF(const XP(DeviceV2) *instance, const char *name);

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        /**
         * @brief MoveAbsFocuser Move to an absolute target position
         * @param targetTicks target position
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;

        /**
         * @brief MoveRelFocuser Move focuser for a relative amount of ticks in a specific direction
         * @param dir Directoin of motion
         * @param ticks steps to move
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

        /**
         * @brief SyncFocuser Set the supplied position as the current focuser position
         * @param ticks target position
         * @return IPS_OK if focuser position is now set to ticks. IPS_ALERT for problems.
         */
        virtual bool SyncFocuser(uint32_t ticks) override;

        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

    private:
		bool isMoving();
		bool readReverse();
		bool readBeep();
		bool readBacklash();
		bool readPosition();
		bool readMaxPosition();
		bool readTemperature();
		
        // Read Only Temperature Reporting
        INDI::PropertyNumber TemperatureNP{1};

        // Beep
        INDI::PropertySwitch BeepSP{2};
        enum
        {
            BEEP_ON,
            BEEP_OFF
        };

        INDI::PropertyText VersionTP {4};
        enum
        {
            TC_FW_VERSION,
            TC_HW_VERSION,
            TC_REV,
            TC_SDK
        };

        THAND m_Handle { nullptr };
        const XP(DeviceV2) *m_Instance;
};
