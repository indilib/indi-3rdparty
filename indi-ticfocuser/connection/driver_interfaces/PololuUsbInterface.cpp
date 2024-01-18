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

#include "PololuUsbInterface.h"

#include <tic.h>
#include <cstring>

bool PololuUsbInterface::energize()
{
    tic_error* err = tic_energize(handle);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";

	return true;
}

bool PololuUsbInterface::deenergize()
{
    tic_error* err = tic_deenergize(handle);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";
    
    return true;

}

bool PololuUsbInterface::exitSafeStart()
{
    tic_error* err = tic_exit_safe_start(handle);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";
    
    return true;
}

bool PololuUsbInterface::haltAndHold()
{
    tic_error* err = tic_halt_and_hold(handle);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";
    
    return true;
}

bool PololuUsbInterface::setTargetPosition(int position)
{
    tic_error* err = tic_set_target_position(handle, position);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";
    
    return true;
}

bool PololuUsbInterface::haltAndSetPosition(int position)
{
    tic_error* err = tic_halt_and_set_position(handle, position);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";
    
    return true;
}

bool PololuUsbInterface::getVariables(TicVariables* vars)
{
    tic_variables* variables = NULL;

    tic_error* err  = tic_get_variables(handle,&variables,false);
    if (err) {
        lastErrorMsg = tic_error_get_message(err);
        tic_error_free(err);
        return false;
    }
    lastErrorMsg = "OK";

    vars->currentPosition = tic_variables_get_current_position(variables);
    vars->targetPosition = tic_variables_get_target_position(variables);

    vars->vinVoltage = tic_variables_get_vin_voltage(variables);
    vars->currentLimit = tic_variables_get_current_limit(variables);
    vars->energized = tic_variables_get_energized(variables);
    vars->stepMode = tic_look_up_step_mode_name_ui(
        tic_variables_get_step_mode(variables));
    vars->operationalState = tic_look_up_operation_state_name_ui(
        tic_variables_get_operation_state(variables));

    vars->errorStatus = tic_variables_get_error_status(variables);

    tic_variables_free(variables);
    return true;
}
