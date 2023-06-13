/*
 * $Id$
 *
 * Copyright (C) 2007, Arnim Laeuger
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
 * Written by Arnim Laeuger <arniml@users.sourceforge.net>, 2007.
 *
 */

#ifndef URJ_BSDL_BSDL_H
#define URJ_BSDL_BSDL_H

#include "types.h"

#include "bsdl_mode.h"

typedef struct
{
    char **path_list;
    int debug;
}
urj_bsdl_globs_t;

#define URJ_BSDL_GLOBS_INIT(bsdl) \
    do { \
        bsdl.path_list = NULL; \
        bsdl.debug = 0; \
    } while (0)

/* @@@@ RFHH ToDo: let urj_bsdl_read_file also return URJ_STATUS_... */
/**
 * @return
 *   < 0 : Error occured, parse/syntax problems or out of memory
 *   = 0 : No errors, idcode not checked or mismatching
 *   > 0 : No errors, idcode checked and matched
 */
int urj_bsdl_read_file (urj_chain_t *, const char *, int, const char *);
void urj_bsdl_set_path (urj_chain_t *, const char *);
/* @@@@ RFHH ToDo: let urj_bsdl_scan_files also return URJ_STATUS_... */
/**
 * @return
 *   < 0 : Error occured, parse/syntax problems or out of memory
 *   = 0 : No errors, idcode not checked or mismatching
 *   > 0 : No errors, idcode checked and matched
 */
int urj_bsdl_scan_files (urj_chain_t *, const char *, int);

#endif /* URJ_BSDL_BSDL_H */
