/*
 Copyright (C) 2018-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <string>

#ifdef BUILD_TOUPCAM
#include <toupcam.h>
#define FP(x) Toupcam_##x
#define CP(x) TOUPCAM_##x
#define XP(x) Toupcam##x
#define THAND HToupcam
#define DNAME "ToupTek"
#elif BUILD_ALTAIRCAM
#include <altaircam.h>
#define FP(x) Altaircam_##x
#define CP(x) ALTAIRCAM_##x
#define XP(x) Altaircam##x
#define THAND HAltaircam
#define DNAME "Altair"
#elif BUILD_BRESSERCAM
#include <bressercam.h>
#define FP(x) Bressercam_##x
#define CP(x) BRESSERCAM_##x
#define XP(x) Bressercam##x
#define THAND HBressercam
#define DNAME "Bresser"
#elif BUILD_MALLINCAM
#include <mallincam.h>
#define FP(x) Mallincam_##x
#define CP(x) MALLINCAM_##x
#define XP(x) Mallincam##x
#define THAND HMallincam
#define DNAME "MALLINCAM"
#elif BUILD_NNCAM
#include <nncam.h>
#define FP(x) Nncam_##x
#define CP(x) NNCAM_##x
#define XP(x) Nncam##x
#define THAND HNncam
#define DNAME "Nn"
#elif BUILD_OGMACAM
#include <ogmacam.h>
#define FP(x) Ogmacam_##x
#define CP(x) OGMACAM_##x
#define XP(x) Ogmacam##x
#define THAND HOgmacam
#define DNAME "OGMAVision"
#elif BUILD_OMEGONPROCAM
#include <omegonprocam.h>
#define FP(x) Omegonprocam_##x
#define CP(x) OMEGONPROCAM_##x
#define XP(x) Omegonprocam##x
#define THAND HOmegonprocam
#define DNAME "Astroshop"
#elif BUILD_STARSHOOTG
#include <starshootg.h>
#define FP(x) Starshootg_##x
#define CP(x) STARSHOOTG_##x
#define XP(x) Starshootg##x
#define THAND HStarshootg
#define DNAME "Orion"
#elif BUILD_TSCAM
#include <tscam.h>
#define FP(x) Tscam_##x
#define CP(x) TSCAM_##x
#define XP(x) Tscam##x
#define THAND HTscam
#define DNAME "Teleskop"
#elif BUILD_SVBONYCAM
#include <svbonycam.h>
#define FP(x) Svbonycam_##x
#define CP(x) SVBONYCAM_##x
#define XP(x) Svbonycam##x
#define THAND HSvbonycam
#define DNAME "SVBONY2"
#elif BUILD_MEADECAM
#include <meadecam.h>
#define FP(x) Toupcam_##x
#define CP(x) TOUPCAM_##x
#define XP(x) Toupcam##x
#define THAND HToupcam
#define DNAME "Meade"
#endif

extern std::string errorCodes(HRESULT rc);
