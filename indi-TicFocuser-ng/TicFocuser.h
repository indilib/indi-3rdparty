/*******************************************************************************
TicFocuser
Copyright (C) 2019-2021 Sebastian Baberowski

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#ifndef TICFOCUSER_H
#define TICFOCUSER_H

#include <indifocuser.h>

class TicFocuser : public INDI::Focuser
{ 
    public:
        
        TicFocuser();
        virtual ~TicFocuser();

        const char *getDefaultName() {  return "TIC Focuser NG"; }

        virtual bool initProperties();
        virtual bool updateProperties();        

        bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

        bool saveConfigItems(FILE *fp);

        bool Disconnect();
        bool Connect();

        IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        IPState MoveAbsFocuser(uint32_t ticks);
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
        bool AbortFocuser();
        bool SyncFocuser(uint32_t ticks);

        void TimerHit();

        bool energizeFocuser();
        bool deenergizeFocuser();
    
    protected:
        virtual bool SetFocuserBacklash(int32_t steps) override;

    private:

        bool lastTimerHitError;      //< used to not flood user with the same error messge if it repeats
        int32_t moveRelInitialValue; //< used to simulate MoveRelFocuser
        FocusDirection lastFocusDir; //< used to identify direction reversal (for backlash)

        ISwitch FocusParkingModeS[2];
        ISwitchVectorProperty FocusParkingModeSP;

        ISwitch EnergizeFocuserS[2];
        ISwitchVectorProperty EnergizeFocuserSP;

        enum InfoTab {
            VIN_VOLTAGE,
            CURRENT_LIMIT,
            ENERGIZED,
            STEP_MODE,
            OPERATION_STATE,

            InfoTabSize
        };

        IText InfoS[InfoTabSize] = {};
        ITextVectorProperty InfoSP;

        IText* InfoErrorS;
        ITextVectorProperty InfoErrorSP;
};

#endif // TICFOCUSER_H
