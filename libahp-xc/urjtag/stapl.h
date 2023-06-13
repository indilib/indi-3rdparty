/*
 * $Id$
 *
 * Copyright (C) 2011, Michael Vacek
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
 * Written by Michael Vacek <michael.vacek@gmail.com>, 2011.
 *
 */

#ifndef URJ_STAPL_H
#define URJ_STAPL_H

#include "types.h"

int urj_stapl_run (urj_chain_t *chain, char *STAPL_file_name,
                   char *STAPL_action);

#endif /* URJ_STAPL_H */
