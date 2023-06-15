/*
 * $Id$
 *
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

#ifndef URJ_JTAG_H
#define URJ_JTAG_H

#include "types.h"

typedef enum URJ_ENDIAN {
    URJ_ENDIAN_LITTLE,
    URJ_ENDIAN_BIG,
    URJ_ENDIAN_UNKNOWN,
} urj_endian_t;

/**
 * Get the endian used in external files.  See cmd_endian.c.
 */
urj_endian_t urj_get_file_endian (void);

/**
 * Set the endian used in external files.  See cmd_endian.c.
 */
void urj_set_file_endian (urj_endian_t);

/**
 * Return the string representation of an endian type.
 */
const char *urj_endian_to_string (urj_endian_t);

/**
 * Convert an endian string representation into the normal type.
 */
urj_endian_t urj_endian_from_string (const char *);

/**
 * Register the application name with global/data_dir.
 * @param argv0 is remembered as a pointer, it is not strdup()'ed.
 */
void urj_set_argv0(const char *argv0);
const char *urj_get_data_dir (void);

#endif /* URJ_JTAG_H */
