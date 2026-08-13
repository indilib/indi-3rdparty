/*
    ScopeLink INDI driver - build time configuration

    Copyright (C) 2026 Astrolabs Hungary Kft.

    Owner:      Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>
    Maintainer: Bence Toth (Astrolabs Hungary Kft.) <bence.toth@astrolabs.hu>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
*/

#ifndef CONFIG_H
#define CONFIG_H

/* Define INDI Data Dir */
#cmakedefine INDI_DATA_DIR "@INDI_DATA_DIR@"

/* Define Driver version */
#define SCOPELINK_VERSION_MAJOR @SCOPELINK_VERSION_MAJOR@
#define SCOPELINK_VERSION_MINOR @SCOPELINK_VERSION_MINOR@

#endif // CONFIG_H
