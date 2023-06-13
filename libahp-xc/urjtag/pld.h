/*
 * $Id$
 *
 * Support for Programmable Logic Devices (PLD)
 *
 * Copyright (C) 2010, Michael Walle
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
 * Written by Michael Walle <michael@walle.cc>, 2010
 *
 */

#ifndef URJ_PLD_H
#define URJ_PLD_H

#include <stdint.h>
#include <stdio.h>

#include "types.h"

typedef struct
{
    urj_chain_t *chain;
    urj_part_t *part;
    void *priv;
} urj_pld_t;

typedef struct
{
    const char *name;
    int (*detect) (urj_pld_t *pld);
    int (*print_status) (urj_pld_t *pld);
    int (*configure) (urj_pld_t *pld, FILE *pld_file);
    int (*reconfigure) (urj_pld_t *pld);
    int (*read_register) (urj_pld_t *pld, uint32_t reg, uint32_t *value);
    int (*write_register) (urj_pld_t *pld, uint32_t reg, uint32_t value);
    int register_width;
} urj_pld_driver_t;

/**
 * urj_pld_configure(chain, pld_file)
 *
 * Main entry point for the 'pld load' command.
 *
 * XXX Description here
 *
 * @param chain            pointer to global chain
 * @param pld_file         file handle of PLD file
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 */
int urj_pld_configure (urj_chain_t *chain, FILE *pld_file);

/**
 * urj_pld_reconfigure(chain)
 *
 * Main entry point for the 'pld reconfigure' command.
 *
 * XXX Description here
 *
 * @param chain            pointer to global chain
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 */
int urj_pld_reconfigure (urj_chain_t *chain);

/**
 * urj_pld_status(chain)
 *
 * Main entry point for the 'pld status' command.
 *
 * XXX Description here
 *
 * @param chain            pointer to global chain
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 */
int urj_pld_print_status (urj_chain_t *chain);

/**
 * urj_pld_read_register(chain)
 *
 * Main entry point for the 'pld readreg' command.
 *
 * XXX Description here
 *
 * @param chain            pointer to global chain
 * @param reg              register number
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 */
int urj_pld_read_register (urj_chain_t *chain, uint32_t reg);

/**
 * urj_pld_write_register(chain)
 *
 * Main entry point for the 'pld writereg' command.
 *
 * XXX Description here
 *
 * @param chain            pointer to global chain
 * @param reg              register number
 * @param value            value
 *
 * @return
 *   URJ_STATUS_OK, URJ_STATUS_FAIL
 */
int urj_pld_write_register (urj_chain_t *chain, uint32_t reg, uint32_t value);

#endif /* URJ_PLD_H */
