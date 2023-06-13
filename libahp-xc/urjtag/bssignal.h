/*
 * $Id$
 *
 * Copyright (C) 2002 ETC s.r.o.
 * Copyright (C) 2003 Marcel Telka
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

#ifndef URJ_BSSIGNAL_H
#define URJ_BSSIGNAL_H

#include "types.h"

struct URJ_PART_SIGNAL
{
    char *name;
    char *pin;                  /* djf hack pin number from bsdl */
    urj_part_signal_t *next;
    urj_bsbit_t *input;
    urj_bsbit_t *output;
};

struct URJ_PART_SALIAS
{
    char *name;
    urj_part_salias_t *next;
    urj_part_signal_t *signal;
};

urj_part_signal_t *urj_part_signal_alloc (const char *name);
void urj_part_signal_free (urj_part_signal_t *s);

urj_part_salias_t *urj_part_salias_alloc (const char *name,
                                          const urj_part_signal_t *signal);
void urj_part_salias_free (urj_part_salias_t *salias);

/**
 * Define a signal and its associated pin (name)
 */
urj_part_signal_t *urj_part_signal_define_pin (urj_chain_t *chain,
                                               const char *signal_name,
                                               const char *pin_name);
/**
 * Define a signal without pin (name)
 */
urj_part_signal_t *urj_part_signal_define (urj_chain_t *chain,
                                           const char *signal_name);
/**
 * Redefine the pin name for a signal
 */
int urj_part_signal_redefine_pin (urj_chain_t *chain, urj_part_signal_t *s,
                                  const char *pin_name);

#endif /* URJ_BSSIGNAL_H */
