/*
 * $Id$
 *
 * Copyright (C) 2009, Rutger Hofman, VU Amsterdam
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

#ifndef URJ_ERROR_H
#define URJ_ERROR_H

#include <stdio.h>
#include <errno.h>

#include "log.h"

/**
 * Error types
 */
typedef enum URJ_ERROR
{
    URJ_ERROR_OK        = 0,
    URJ_ERROR_ALREADY,
    URJ_ERROR_OUT_OF_MEMORY,
    URJ_ERROR_NO_CHAIN,
    URJ_ERROR_NO_PART,
    URJ_ERROR_NO_ACTIVE_INSTRUCTION,
    URJ_ERROR_NO_DATA_REGISTER,
    URJ_ERROR_INVALID,
    URJ_ERROR_NOTFOUND,
    URJ_ERROR_NO_BUS_DRIVER,
    URJ_ERROR_BUFFER_EXHAUSTED,
    URJ_ERROR_ILLEGAL_STATE,
    URJ_ERROR_ILLEGAL_TRANSITION,
    URJ_ERROR_OUT_OF_BOUNDS,
    URJ_ERROR_TIMEOUT,
    URJ_ERROR_UNSUPPORTED,
    URJ_ERROR_SYNTAX,
    URJ_ERROR_FILEIO,                   /**< I/O error from fread/fwrite */

    URJ_ERROR_IO,                       /**< I/O error from OS */
    URJ_ERROR_FTD,                      /**< error from ftdi/ftd2xx */
    URJ_ERROR_USB,                      /**< error from libusb */

    URJ_ERROR_BUS,
    URJ_ERROR_BUS_DMA,

    URJ_ERROR_FLASH,
    URJ_ERROR_FLASH_DETECT,
    URJ_ERROR_FLASH_PROGRAM,
    URJ_ERROR_FLASH_ERASE,
    URJ_ERROR_FLASH_LOCK,
    URJ_ERROR_FLASH_UNLOCK,

    URJ_ERROR_BSDL_VHDL,
    URJ_ERROR_BSDL_BSDL,

    URJ_ERROR_BFIN,

    URJ_ERROR_PLD,

    URJ_ERROR_UNIMPLEMENTED,

    URJ_ERROR_FIRMWARE,
}
urj_error_t;

/** Max length of message string that can be recorded. */
#define URJ_ERROR_MSG_LEN       256

/**
 * Error state.
 */
typedef struct URJ_ERROR_STATE
{
    urj_error_t         errnum;                 /**< error number */
    int                 sys_errno;              /**< errno if URJ_ERROR_IO */
    const char         *file;                   /**< file where error is set */
    const char         *function;               /**< function --,,-- */
    int                 line;                   /**< line no --,,-- */
    char                msg[URJ_ERROR_MSG_LEN]; /**< printf-style message */
}
urj_error_state_t;

extern urj_error_state_t        urj_error_state;

/**
 * Descriptive string for error type
 */
extern const char *urj_error_string (urj_error_t error);

/**
 * Set error state.
 *
 * @param e urj_error_t value
 * @param ... error detail message that consists of a printf argument set.
 *      It needs to start with a const char *fmt, followed by arguments used
 *      by fmt.
 */
#define urj_error_set(e, ...) \
    do { \
        urj_error_state.errnum = e; \
        urj_error_state.file = __FILE__; \
        urj_error_state.function = __func__; \
        urj_error_state.line = __LINE__; \
        snprintf (urj_error_state.msg, sizeof urj_error_state.msg, \
                  __VA_ARGS__); \
    } while (0)

/**
 * Set I/O error state: do as urj_error_set, but also store errno in
 * #urj_error_state and then reset errno.
 *
 * @param ... error detail message that consists of a printf argument set.
 *      It needs to start with a const char *fmt, followed by arguments used
 *      by fmt. The error code (URJ_ERROR_IO) is added by this macro.
 */
#define urj_error_IO_set(...) \
    do { \
        urj_error_set (URJ_ERROR_IO, __VA_ARGS__); \
        urj_error_state.sys_errno = errno; \
        errno = 0; \
    } while (0)

/**
 * The error number
 */
urj_error_t urj_error_get (void);

/**
 * Reset the error state.
 */
void urj_error_reset (void);
/**
 * The error state in human-readable form.
 *
 * This function is not reentrant.
 *
 * @return a pointer to a static area.
 */
const char *urj_error_describe (void);

#endif /* URJ_ERROR_H */
