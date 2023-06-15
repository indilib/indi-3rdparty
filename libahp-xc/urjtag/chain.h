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

#ifndef URJ_CHAIN_H
#define URJ_CHAIN_H

#include "types.h"

#include "pod.h"
#include "bsdl.h"
#include "error.h"

#define URJ_CHAIN_EXITMODE_SHIFT        0
#define URJ_CHAIN_EXITMODE_IDLE         1
#define URJ_CHAIN_EXITMODE_EXIT1        2
#define URJ_CHAIN_EXITMODE_UPDATE       3

struct URJ_CHAIN
{
    int state;
    urj_parts_t *parts;
    int total_instr_len;
    int active_part;
    urj_cable_t *cable;
    urj_bsdl_globs_t bsdl;
    int main_part;
};

urj_chain_t *urj_tap_chain_alloc (void);
void urj_tap_chain_free (urj_chain_t *chain);
/**
 * Connect the chain to the specified cable.
 *
 * @param chain      chain object to connect
 * @param drivername name of cable driver
 * @param params     additional driver-specific parameters
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_tap_chain_connect (urj_chain_t *chain, const char *drivername, char *params[]);
void urj_tap_chain_disconnect (urj_chain_t *chain);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_clock (urj_chain_t *chain, int tms, int tdi, int n);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_defer_clock (urj_chain_t *chain, int tms, int tdi, int n);
/** @return trst = 0 or 1 on success; -1 on error */
int urj_tap_chain_set_trst (urj_chain_t *chain, int trst);
/** @return 0 or 1 on success; -1 on error */
int urj_tap_chain_get_trst (urj_chain_t *chain);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_shift_instructions (urj_chain_t *chain);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_shift_instructions_mode (urj_chain_t *chain,
                                           int capture_output, int capture,
                                           int chain_exit);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_shift_data_registers (urj_chain_t *chain,
                                        int capture_output);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_tap_chain_shift_data_registers_mode (urj_chain_t *chain,
                                             int capture_output, int capture,
                                             int chain_exit);
void urj_tap_chain_flush (urj_chain_t *chain);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_chain_set_pod_signal (urj_chain_t *chain, int mask, int val);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_chain_get_pod_signal (urj_chain_t *chain, urj_pod_sigsel_t sig);
/**
 * Check whether a chain has an active part
 *
 * @return NULL on error, and sets urj_error.
 */
urj_part_t *urj_tap_chain_active_part (urj_chain_t *chain);
void urj_tap_chain_wait_ready (urj_chain_t *chain);

typedef struct
{
    urj_chain_t **chains;
    int size;                   /* allocated chains array size */
}
urj_chains_t;

#endif /* URJ_CHAIN_H */
