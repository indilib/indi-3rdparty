/*******************************************************************************
TicFocuser
Copyright (C) 2019 Sebastian Baberowski

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

#include "TiclibInterface.h"

#include "connection/ticlib/TicBase.h"
#include "connection/ticlib/TicDefs.h"

#include <cstring>

bool TiclibInterface::energize()
{
    ticBase.energize();
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "Energize error";
        return false;
    }

    lastErrorMsg = "OK";
	return true;
}

bool TiclibInterface::deenergize()
{
     ticBase.deenergize();
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "De-energize error";
        return false;
    }

    lastErrorMsg = "OK";
    return true;
}

bool TiclibInterface::exitSafeStart()
{
    ticBase.exitSafeStart();
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "ExitSafeStart error";
        return false;
    }

    lastErrorMsg = "OK";
    return true;
}

bool TiclibInterface::haltAndHold()
{
    ticBase.haltAndHold();
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "HaltAndHold error";
        return false;
    }

    lastErrorMsg = "OK";
    return true;
}

bool TiclibInterface::setTargetPosition(int position)
{
    ticBase.setTargetPosition(position);
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "SetTargetPosition error";
        return false;
    }

    lastErrorMsg = "OK";
    return true;
}

bool TiclibInterface::haltAndSetPosition(int position)
{
    ticBase.haltAndSetPosition(position);
    if (ticBase.getLastError()) 
    {
        lastErrorMsg = "HaltAndSetPosition error";
        return false;
    }

    lastErrorMsg = "OK";
    return true;
}

bool TiclibInterface::getVariables(TicVariables* vars)
{
    vars->targetPosition = ticBase.getTargetPosition();
    if (ticBase.getLastError())
    {
        lastErrorMsg = "GetTargetPosition error";
        return false;
    }

    vars->currentPosition = ticBase.getCurrentPosition();
    if (ticBase.getLastError())
    {
        lastErrorMsg = "GetCurrentPosition error";
        return false;
    }


    vars->vinVoltage = ticBase.getVinVoltage();
    vars->currentLimit = ticBase.getCurrentLimit();
    vars->energized = ticBase.getEnergized();

    vars->stepMode = tic_look_up_step_mode_name_ui((int)ticBase.getStepMode());
    vars->operationalState = tic_look_up_operation_state_name_ui((int)ticBase.getOperationState());

    vars->errorStatus = ticBase.getErrorStatus();

    lastErrorMsg = "OK";
    return true;
}

