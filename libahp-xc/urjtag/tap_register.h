/*
 * $Id$
 *
 * Copyright (C) 2002, 2003 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2002, 2003.
 *
 */

#ifndef URJ_TAP_REGISTER_H
#define URJ_TAP_REGISTER_H

#include "types.h"
#include <stdint.h>

struct URJ_TAP_REGISTER
{
    char *data;         /* (public, r/w) register data */
    int len;            /* (public, r/o) register length */
    char *string;       /* (private) string representation of register data */
};

urj_tap_register_t *urj_tap_register_alloc (int len);
urj_tap_register_t *urj_tap_register_realloc (urj_tap_register_t *tr, int new_len);
urj_tap_register_t *urj_tap_register_duplicate (const urj_tap_register_t *tr);
void urj_tap_register_free (urj_tap_register_t *tr);
urj_tap_register_t *urj_tap_register_fill (urj_tap_register_t *tr, int val);
int urj_tap_register_set_string (urj_tap_register_t *tr, const char *str);
int urj_tap_register_set_string_bit_range (urj_tap_register_t *tr, const char *str, int msb, int lsb);
int urj_tap_register_set_value (urj_tap_register_t *tr, uint64_t val);
int urj_tap_register_set_value_bit_range (urj_tap_register_t *tr, uint64_t val, int msb, int lsb);
const char *urj_tap_register_get_string (const urj_tap_register_t *tr);
const char *urj_tap_register_get_string_bit_range (const urj_tap_register_t *tr, int msb, int lsb);
uint64_t urj_tap_register_get_value (const urj_tap_register_t *tr);
uint64_t urj_tap_register_get_value_bit_range (const urj_tap_register_t *tr, int msb, int lsb);
/** @return 0 or 1 on success; -1 on error */
int urj_tap_register_all_bits_same_value (const urj_tap_register_t *tr);
urj_tap_register_t *urj_tap_register_init (urj_tap_register_t *tr,
                                           const char *value);
int urj_tap_register_compare (const urj_tap_register_t *tr,
                              const urj_tap_register_t *tr2);
int urj_tap_register_match (const urj_tap_register_t *tr, const char *expr);
urj_tap_register_t *urj_tap_register_inc (urj_tap_register_t *tr);
urj_tap_register_t *urj_tap_register_dec (urj_tap_register_t *tr);
urj_tap_register_t *urj_tap_register_shift_right (urj_tap_register_t *tr,
                                                  int shift);
urj_tap_register_t *urj_tap_register_shift_left (urj_tap_register_t *tr,
                                                 int shift);

#endif /* URJ_REGISTER_H */
