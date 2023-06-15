/* include/urjtag/urjtag.h.  Generated from urjtag.h.in by configure.  */
/*
 * $Id$
 *
 * Public include file for the UrJTAG library.
 * Copyright (C) 2009, Rutger Hofman
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
 * Author: Rutger Hofman, VU Amsterdam
 *
 */

#ifndef URJ_URJTAG_H
#define URJ_URJTAG_H

#define ENABLE_BSDL 1
#define ENABLE_SVF 1
#define HAVE_LIBUSB 1

#include "types.h"
#include "bitmask.h"
#include "bsbit.h"
#if ENABLE_BSDL
#include "bsdl.h"
#include "bsdl_mode.h"
#endif
#include "bssignal.h"
#include "bus.h"
#include "bus_driver.h"
#include "cable.h"
#include "chain.h"
#include "cmd.h"
#include "data_register.h"
#include "fclock.h"
#include "flash.h"
#include "gettext.h"
#include "jim.h"
#include "jtag.h"
#include "parport.h"
#include "part.h"
#include "part_instruction.h"
#include "pod.h"
#if ENABLE_SVF
#include "svf.h"
#endif
#include "tap.h"
#include "tap_register.h"
#include "tap_state.h"
#if HAVE_LIBUSB
#include "usbconn.h"
#endif

#endif /* URJ_URJTAG_H */
