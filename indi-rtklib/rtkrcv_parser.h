/*******************************************************************************
  Copyright(c) 2020 Ilia Platone - Jasem Mutlaq. All rights reserved.

  INDI RTKLIB Driver

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

#ifndef RTKRCV_H
#define RTKRCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define RTKRCV_MAX_LENGTH 150

#define RTKRCV_FIX_NONE "------"
#define RTKRCV_FIX "FIX"
#define RTKRCV_FIX_FLOAT "FLOAT"
#define RTKRCV_FIX_SBAS "SBAS"
#define RTKRCV_FIX_DGPS "DGPS"
#define RTKRCV_FIX_SINGLE "SINGLE"
#define RTKRCV_FIX_PPP "PPP"
#define RTKRCV_FIX_UNKNOWN ""

enum rtkrcv_fix_status {
    status_no_fix=1,
    status_fix,
    status_float,
    status_sbas,
    status_dgps,
    status_single,
    status_ppp,
    status_unknown
};

void scansolution(char* solution, char *flags, char *type, double *dms, enum rtkrcv_fix_status *fix, double *timestamp);

#ifdef __cplusplus
}
#endif

#endif /* RTKRCV_H */

/* vim: set ts=4 sw=4 et: */
