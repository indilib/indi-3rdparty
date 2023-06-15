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

#ifndef URJ_DATA_REGISTER_H
#define URJ_DATA_REGISTER_H

#include "types.h"

#define URJ_DATA_REGISTER_MAXLEN        32

struct URJ_DATA_REGISTER
{
    char name[URJ_DATA_REGISTER_MAXLEN + 1];    /* (public) register name */
    urj_tap_register_t *in;     /* (public) register value clocked in */
    urj_tap_register_t *out;    /* (public) register value clocked out */
    urj_data_register_t *next;
};

urj_data_register_t *urj_part_data_register_alloc (const char *name, int len);
void urj_part_data_register_free (urj_data_register_t *dr);

/**
 * change the length of a data_register while preserving its contents
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_part_data_register_realloc (urj_data_register_t *dr, int len);

/**
 * allocate a data register and initialize the relevant <code>part</code> parts
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_part_data_register_define (urj_part_t *part, const char *name, int len);

#endif /* URJ_DATA_REGISTER_H */
