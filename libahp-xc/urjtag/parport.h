/*
 * $Id$
 *
 * Parallel Port Connection Driver Interface
 * Copyright (C) 2003 ETC s.r.o.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Marcel Telka <marcel@telka.sk>, 2003.
 *
 */

#ifndef URJ_PARPORT_H
#define URJ_PARPORT_H

#include <stdint.h>

#include "types.h"
#include "params.h"

typedef enum URJ_CABLE_PARPORT_DEVTYPE
{
    URJ_CABLE_PARPORT_DEV_PARALLEL,
    URJ_CABLE_PARPORT_DEV_PPDEV,
    URJ_CABLE_PARPORT_DEV_PPI,
    URJ_CABLE_PARPORT_N_DEVS,
}
urj_cable_parport_devtype_t;

typedef struct
{
    urj_cable_parport_devtype_t type;
    urj_parport_t *(*connect) (const char *devname);
    void (*parport_free) (urj_parport_t *);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*open) (urj_parport_t *);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*close) (urj_parport_t *);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*set_data) (urj_parport_t *, unsigned char);
    /** @return data on success; -1 on error */
    int (*get_data) (urj_parport_t *);
    /** @return status on success; -1 on error */
    int (*get_status) (urj_parport_t *);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*set_control) (urj_parport_t *, unsigned char);
}
urj_parport_driver_t;

struct URJ_PARPORT
{
    const urj_parport_driver_t *driver;
    void *params;
    urj_cable_t *cable;
};

int urj_tap_parport_open (urj_parport_t *port);
int urj_tap_parport_close (urj_parport_t *port);
int urj_tap_parport_set_data (urj_parport_t *port, const unsigned char data);
/** @return data on success; -1 on error */
int urj_tap_parport_get_data (urj_parport_t *port);
/** @return status on success; -1 on error */
int urj_tap_parport_get_status (urj_parport_t *port);
int urj_tap_parport_set_control (urj_parport_t *port, const unsigned char data);

const char *urj_cable_parport_devtype_string(urj_cable_parport_devtype_t dt);

extern const urj_parport_driver_t * const urj_tap_parport_drivers[];

#endif /* URJ_PARPORT_H */
