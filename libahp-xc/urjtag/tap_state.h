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

#ifndef URJ_TAP_STATE_H
#define URJ_TAP_STATE_H

#include "bitmask.h"

#include "types.h"

#define URJ_TAP_STATE_DR        URJ_BIT (0)
#define URJ_TAP_STATE_IR        URJ_BIT (1)
#define URJ_TAP_STATE_SHIFT     URJ_BIT (2)     /* register shift with TMS = 0 */
#define URJ_TAP_STATE_IDLE      URJ_BIT (3)     /* to Run-Test/Idle with TMS = 0 */
#define URJ_TAP_STATE_CAPTURE   URJ_BIT (4)     /* Capture state */
#define URJ_TAP_STATE_UPDATE    URJ_BIT (5)     /* to Update with TMS = 1 */
#define URJ_TAP_STATE_PAUSE     URJ_BIT (6)     /* to Pause with TMS = 0 */
#define URJ_TAP_STATE_RESET     URJ_BIT (7)     /* Test-Logic-Reset or unknown state */

#define URJ_TAP_STATE_UNKNOWN_STATE     URJ_TAP_STATE_RESET
#define URJ_TAP_STATE_TEST_LOGIC_RESET  (URJ_TAP_STATE_RESET | URJ_TAP_STATE_IDLE)
#define URJ_TAP_STATE_RUN_TEST_IDLE     URJ_TAP_STATE_IDLE
#define URJ_TAP_STATE_SELECT_DR_SCAN    URJ_TAP_STATE_DR
#define URJ_TAP_STATE_CAPTURE_DR        (URJ_TAP_STATE_DR | URJ_TAP_STATE_SHIFT | URJ_TAP_STATE_CAPTURE)
#define URJ_TAP_STATE_SHIFT_DR          (URJ_TAP_STATE_DR | URJ_TAP_STATE_SHIFT)
#define URJ_TAP_STATE_EXIT1_DR          (URJ_TAP_STATE_DR | URJ_TAP_STATE_UPDATE | URJ_TAP_STATE_PAUSE)
#define URJ_TAP_STATE_PAUSE_DR          (URJ_TAP_STATE_DR | URJ_TAP_STATE_PAUSE)
#define URJ_TAP_STATE_EXIT2_DR          (URJ_TAP_STATE_DR | URJ_TAP_STATE_SHIFT | URJ_TAP_STATE_UPDATE)
#define URJ_TAP_STATE_UPDATE_DR         (URJ_TAP_STATE_DR | URJ_TAP_STATE_IDLE)
#define URJ_TAP_STATE_SELECT_IR_SCAN    URJ_TAP_STATE_IR
#define URJ_TAP_STATE_CAPTURE_IR        (URJ_TAP_STATE_IR | URJ_TAP_STATE_SHIFT | URJ_TAP_STATE_CAPTURE)
#define URJ_TAP_STATE_SHIFT_IR          (URJ_TAP_STATE_IR | URJ_TAP_STATE_SHIFT)
#define URJ_TAP_STATE_EXIT1_IR          (URJ_TAP_STATE_IR | URJ_TAP_STATE_UPDATE | URJ_TAP_STATE_PAUSE)
#define URJ_TAP_STATE_PAUSE_IR          (URJ_TAP_STATE_IR | URJ_TAP_STATE_PAUSE)
#define URJ_TAP_STATE_EXIT2_IR          (URJ_TAP_STATE_IR | URJ_TAP_STATE_SHIFT | URJ_TAP_STATE_UPDATE)
#define URJ_TAP_STATE_UPDATE_IR         (URJ_TAP_STATE_IR | URJ_TAP_STATE_IDLE)

int urj_tap_state (urj_chain_t *chain);
int urj_tap_state_init (urj_chain_t *chain);
int urj_tap_state_done (urj_chain_t *chain);
int urj_tap_state_reset (urj_chain_t *chain);
int urj_tap_state_set_trst (urj_chain_t *chain, int old_trst, int new_trst);
int urj_tap_state_clock (urj_chain_t *chain, int tms);

#endif /* URJ_TAP_STATE_H */
