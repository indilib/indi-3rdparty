Observatory Control System (OCS) Indi Driver V1.4
-------------------------------------------------

An Indi driver for the OCS (https://onstep.groups.io/g/onstep-ocs/wiki)
  Copyright (C) 2023-2025 Ed Lee

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
