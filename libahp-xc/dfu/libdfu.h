/*
 * dfu-programmer
 *
 * $Id: dfu.h,v 1.2 2005/09/25 01:27:42 schmidtw Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DFU_H
#define DFU_H

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#include <windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT extern
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>


DLL_EXPORT int dfu_flash(int fd, int *progress, int *finished);
DLL_EXPORT int dfu_flash_filename(const char* filename, int *progress, int *finished);

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* DFU_H */
