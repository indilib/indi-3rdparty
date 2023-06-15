/*
 * $Id$
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
 */

#ifndef URJ_BITOPS_H
#define URJ_BITOPS_H

#include <stdint.h>

static inline uint16_t flip16 (uint16_t v)
{
    int i;
    uint16_t out = 0;

    /* flip bits (from left to right) */
    for (i = 0; i < 16; i++)
        if (v & (1 << i))
            out |= (1 << (15 - i));

    return out;
}

static inline uint32_t flip32 (uint32_t v)
{
    int i;
    uint32_t out = 0;

    /* flip bits (from left to right) */
    for (i = 0; i < 32; i++)
        if (v & (1 << i))
            out |= (1 << (31 - i));

    return out;
}

#endif /* URJ_BITOPS_H */
