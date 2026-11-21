Observatory Control System (OCS) Indi Driver V1.7
-------------------------------------------------

An Indi driver for the OCS (https://onstep.groups.io/g/onstep-ocs/wiki)
  Copyright (C) 2023-2025 Ed Lee

Version 1.7
    Fixed driver crashes on roof faults and on a dome goto with no reply
    Weather sensor present at connection that reads NaN or fails to read is re-polled
    after 10 seconds, and goes to Alert on a second consecutive bad reading
    Fixed rain limits, thermostat setpoints, power device 6 and failed handshake detection
    Weather Refresh and Override switches now work
    Weather parameters no longer duplicate on reconnect
    Shutter no longer stays Busy after an abort
    Dome is only marked parked once the controller reports it
    Sensor reading NaN at connection is kept if it is configured in OCS and has a sensor name
    A wind sensor reading 0 at connection is no longer treated as not fitted
    Rain is only monitored if OCS names its rain sensor, as older OCS firmware misroutes :GR#
    Power and light switches keep their state and show Alert if OCS rejects the command
    Roof last error clears when OCS reports no error
    Renamed misspelt THERMOSTAT_*_SEPOINT elements to THERMOSTAT_*_SETPOINT
    Roof/shutter is only polled, and its controls only shown, if OCS has one
    If OCS stops replying for longer than its roof interlock delays plus 5 seconds, commands fail
    at once and one is retried every 5 seconds
    Added tests against a mock OCS controller, run with ctest when INDI_BUILD_UNITTESTS is on

Version 1.6
    Fixed another shutdown issue

Version 1.5
    Fixed shutdown issue

Version 1.4
    Corrected inactive weather measurement check added incorrectly in V1.1

Version 1.3
    Fix for park state and typo

Version 1.2
    Added shutter control on park / unpark options and fixed Dome Az slaving.

Version 1.1
    Added OCS command protocol change on inactive weather measurements - OCS v3.08a
    Fixed weather measurement polling
    Fixed typo in network connection timeout value
    Fixed arbitary command debug blocks
    Added command/response blocking to prevent out-of-order value updates
    Refactored some if-else ladders to switch cases
    Added debug option for pausing to allow remote debugger to attach
    Tested with OCS v3.09f

Version 1.0
    First release

Version 0.1
    Initial test release
