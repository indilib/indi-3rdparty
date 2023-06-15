/*
 * $Id$
 *
 * Pod signal names
 * Copyright (C) 2008 K. Waschk
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
 */

#ifndef URJ_POD_H
#define URJ_POD_H

typedef enum URJ_POD_SIGSEL
{
    URJ_POD_CS_NONE = 0,        /* no/invalid signal */
    URJ_POD_CS_TDI = (1 << 0),  /* out: JTAG/SPI data in */
    URJ_POD_CS_TCK = (1 << 1),  /* out: JTAG/SPI clock */
    URJ_POD_CS_TMS = (1 << 2),  /* out: JTAG test mode select/SPI slave select */
    URJ_POD_CS_TRST = (1 << 3), /* out: JTAG TAP reset */
    URJ_POD_CS_RESET = (1 << 4),        /* out: system reset */
    URJ_POD_CS_SCK = (1 << 5),  /* out: I2C clock (not yet used) */
    URJ_POD_CS_SDA = (1 << 6),  /* inout: I2C data (not yet used) */
    URJ_POD_CS_SS = (1 << 7),   /* out: SPI slave select (not yet used) */
}
urj_pod_sigsel_t;

#endif /* URJ_POD_H */
