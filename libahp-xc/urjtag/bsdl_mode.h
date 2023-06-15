/*
 * $Id$
 *
 * Copyright (C) 2008, Arnim Laeuger
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
 * Written by Arnim Laeuger <arniml@users.sourceforge.net>, 2008.
 *
 */

#ifndef URJ_BSDL_MODE_H
#define URJ_BSDL_MODE_H

#define URJ_BSDL_MODE_MSG_NOTE     (1 <<  0)
#define URJ_BSDL_MODE_MSG_WARN     (1 <<  1)
#define URJ_BSDL_MODE_MSG_ERR      (1 <<  2)
#define URJ_BSDL_MODE_MSG_FATAL    (1 <<  3)

#define URJ_BSDL_MODE_MSG_ALL      (URJ_BSDL_MODE_MSG_FATAL | \
                                    URJ_BSDL_MODE_MSG_ERR   | \
                                    URJ_BSDL_MODE_MSG_WARN  | \
                                    URJ_BSDL_MODE_MSG_NOTE)
#define URJ_BSDL_MODE_MSG_ALWAYS   URJ_BSDL_MODE_MSG_FATAL

#define URJ_BSDL_MODE_SYN_CHECK    (1 <<  4)
#define URJ_BSDL_MODE_INSTR_PRINT  (1 <<  5)
#define URJ_BSDL_MODE_INSTR_EXEC   (1 <<  6)
#define URJ_BSDL_MODE_IDCODE_CHECK (1 <<  7)
#define URJ_BSDL_MODE_ACTION_ALL   (URJ_BSDL_MODE_SYN_CHECK   | \
                                    URJ_BSDL_MODE_INSTR_PRINT | \
                                    URJ_BSDL_MODE_INSTR_EXEC  | \
                                    URJ_BSDL_MODE_IDCODE_CHECK)

#define URJ_BSDL_MODE_INCLUDE1     (URJ_BSDL_MODE_MSG_ALWAYS)
#define URJ_BSDL_MODE_INCLUDE2     (URJ_BSDL_MODE_SYN_CHECK  | \
                                    URJ_BSDL_MODE_INSTR_EXEC | \
                                    URJ_BSDL_MODE_MSG_WARN   | \
                                    URJ_BSDL_MODE_MSG_ERR    | \
                                    URJ_BSDL_MODE_MSG_FATAL)
#define URJ_BSDL_MODE_DETECT       (URJ_BSDL_MODE_SYN_CHECK    | \
                                    URJ_BSDL_MODE_INSTR_EXEC   | \
                                    URJ_BSDL_MODE_IDCODE_CHECK | \
                                    URJ_BSDL_MODE_MSG_ALWAYS)
#define URJ_BSDL_MODE_TEST         (URJ_BSDL_MODE_SYN_CHECK  | \
                                    URJ_BSDL_MODE_MSG_ALL)
#define URJ_BSDL_MODE_DUMP         (URJ_BSDL_MODE_SYN_CHECK   | \
                                    URJ_BSDL_MODE_INSTR_PRINT | \
                                    URJ_BSDL_MODE_MSG_WARN    | \
                                    URJ_BSDL_MODE_MSG_ERR     | \
                                    URJ_BSDL_MODE_MSG_FATAL)

#endif /* URJ_BSDL_MODE_H */
