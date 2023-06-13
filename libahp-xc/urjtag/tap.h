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

#ifndef URJ_TAP_H
#define URJ_TAP_H

#include "types.h"

void urj_tap_reset (urj_chain_t *chain);
void urj_tap_trst_reset (urj_chain_t *chain);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_reset_bypass (urj_chain_t *chain);
void urj_tap_capture_dr (urj_chain_t *chain);
void urj_tap_capture_ir (urj_chain_t *chain);
void urj_tap_defer_shift_register (urj_chain_t *chain,
                                   const urj_tap_register_t *in,
                                   urj_tap_register_t *out, int tap_exit);
void urj_tap_shift_register_output (urj_chain_t *chain,
                                    const urj_tap_register_t *in,
                                    urj_tap_register_t *out, int tap_exit);
void urj_tap_shift_register (urj_chain_t *chain,
                             const urj_tap_register_t *in,
                             urj_tap_register_t *out, int tap_exit);

/** API functions */
/** @return number of detected parts on success; -1 on error */
int urj_tap_detect_parts (urj_chain_t *chain, const char *db_path, int maxirlen);
/** @return chain length on success; -1 on error */
int urj_tap_manual_add (urj_chain_t *chain, int instr_len);
/** @return register size on success; -1 on error */
int urj_tap_detect_register_size (urj_chain_t *chain, int maxlen);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_discovery (urj_chain_t *chain);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_idcode (urj_chain_t *chain, unsigned int bytes);
/**
 * Convenience function that detects the parts, initialises them to BYPASS,
 * and initialises the bus drivers.
 * maxirlen is the maximum expected length of all concatenated instruction
 * registers on the chain.  If set to 0, a default is assumed.
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_tap_detect (urj_chain_t *chain, int maxirlen);

#endif /* URJ_TAP_H */
