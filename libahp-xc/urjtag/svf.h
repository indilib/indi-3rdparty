/*
 * $Id$
 *
 * Copyright (C) 2004, Arnim Laeuger
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
 * Written by Arnim Laeuger <arniml@users.sourceforge.net>, 2004.
 *
 */

#ifndef URJ_SVF_H
#define URJ_SVF_H

#include <stdint.h>
#include <stdio.h>

#include "types.h"

/**
 * ***************************************************************************
 * urj_svf_run(chain, SVF_FILE, stop_on_mismatch, ref_freq)
 *
 * Main entry point for the 'svf' command. Calls the svf parser.
 *
 * Checks the jtag-environment (availability of SIR instruction and SDR
 * register). Initializes all svf-global variables and performs clean-up
 * afterwards.
 *
 * @param chain            pointer to global chain
 * @param SVF_FILE         file handle of SVF file
 * @param stop_on_mismatch 1 = stop upon tdo mismatch
 *                         0 = continue upon mismatch
 * @param ref_freq         reference frequency for RUNTEST
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 * ***************************************************************************/

int urj_svf_run (urj_chain_t *chain, FILE *SVF_FILE, int stop_on_mismatch,
                 uint32_t ref_freq);

#endif /* URJ_SVF_H */
