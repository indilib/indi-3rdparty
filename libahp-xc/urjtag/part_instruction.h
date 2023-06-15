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

#ifndef URJ_PART_INSTRUCTION_H
#define URJ_PART_INSTRUCTION_H

#include "types.h"

#define URJ_INSTRUCTION_MAXLEN_INSTRUCTION      20

struct URJ_PART_INSTRUCTION
{
    char name[URJ_INSTRUCTION_MAXLEN_INSTRUCTION + 1];
    urj_tap_register_t *value;
    urj_tap_register_t *out;
    urj_data_register_t *data_register;
    urj_part_instruction_t *next;
};

urj_part_instruction_t *urj_part_instruction_alloc (const char *name, int len,
                                                    const char *val);
void urj_part_instruction_free (urj_part_instruction_t *i);

#endif /* URJ_INSTRUCTION_H */
