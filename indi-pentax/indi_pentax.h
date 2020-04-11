/*
 Pentax drivers for Indi
 Copyright (C) 2020 Karl Rees

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

#ifndef INDI_PENTAX_H
#define INDI_PENTAX_H

#define MAX_DEVICES    20   /* Max device cameraCount */

#include "pktriggercord_ccd.h"

#ifndef __aarch64__
#include "pentax_ccd.h"
#include "pentax_event_handler.h"

std::vector<std::shared_ptr<CameraDevice>> registeredSDKCams;
#endif

static int cameraCount = 0;
static INDI::CCD *cameras[MAX_DEVICES];
static char logdevicename[14]= "Pentax Driver";

#endif // INDI_PENTAX_H
