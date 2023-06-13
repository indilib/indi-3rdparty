/*
 * $Id$
 *
 * Copyright (C) 2002 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2002.
 *
 */

#ifndef URJ_BSBIT_BSBIT_H
#define URJ_BSBIT_BSBIT_H

#include "types.h"

#define URJ_BSBIT_INPUT         1
#define URJ_BSBIT_OUTPUT        2
#define URJ_BSBIT_CONTROL       3
#define URJ_BSBIT_INTERNAL      4
#define URJ_BSBIT_BIDIR         5

#define URJ_BSBIT_STATE_Z       (-1)

#define URJ_BSBIT_DONTCARE      '?'

struct URJ_BSBIT
{
    int bit;
    char *name;
    int type;
    urj_part_signal_t *signal;
    int safe;                   /* safe value */
    int control;                /* -1 for none */
    int control_value;
    int control_state;
};

/**
 * Define new BSR (Boundary Scan Register) bit for signal <code>name</code>.
 *
 * @param part
 * @param bit
 * @param name associated signal name
 * @param type URJ_BSBIT_{INPUT|OUTPUT|BIDIR|CONTROL|INTERNAL}
 * @param safe default (safe) value (0|1|URJ_BSBIT_DONTCARE)
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_part_bsbit_alloc (urj_part_t *part, int bit, const char *name,
                          int type, int safe);
/**
 * Define new BSR (Boundary Scan Register) bit for signal <code>name</code>.
 * Additionally, define control bit.
 *
 * @param part
 * @param bit
 * @param name associated signal name
 * @param type URJ_BSBIT_{INPUT|OUTPUT|BIDIR|CONTROL|INTERNAL}
 * @param safe default (safe) value (0|1|URJ_BSBIT_DONTCARE)
 * @param ctrl_num control bit number
 * @param ctrl_val control value
 * @param ctrl_state control state; valid statis is only URJ_BSBIT_STATE_Z
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_part_bsbit_alloc_control (urj_part_t *part, int bit, const char *name,
                                  int type, int safe, int ctrl_num,
                                  int ctrl_val, int ctrl_state);

void urj_part_bsbit_free (urj_bsbit_t *b);

#endif /* URJ_BSBIT_BSBIT_H */
