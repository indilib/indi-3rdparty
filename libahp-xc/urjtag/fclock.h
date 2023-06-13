/*
 * fclock.h
 *
 * Copyright (C) 2005 Hein Roehrig
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


#ifndef URJ_FCLOCK_FCLOCK_H
#define URJ_FCLOCK_FCLOCK_H

/* @@@@ RFHH this had better be an internal include file */


#ifdef __cplusplus
extern "C"
{

#  ifndef CVOID
#    define CVOID
#  endif
#else /* def __cplusplus */
#  ifndef CVOID
#    define CVOID void
#  endif
#endif /* def __cplusplus */


/* return real time in seconds starting at some arbitrary point in
time*/
long double urj_lib_frealtime (CVOID);


#ifdef __cplusplus
}
#endif


#endif
