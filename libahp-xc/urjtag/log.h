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

#ifndef URJ_LOG_H
#define URJ_LOG_H

#include <stdarg.h>

#include "types.h"

/**
 * Log state.
 */
typedef struct URJ_LOG_STATE
{
    urj_log_level_t     level;                  /**< logging level */
    int               (*out_vprintf) (const char *fmt, va_list ap);
    int               (*err_vprintf) (const char *fmt, va_list ap);
}
urj_log_state_t;

extern urj_log_state_t urj_log_state;

int urj_do_log (urj_log_level_t level, const char *file, size_t line,
                const char *func, const char *fmt, ...)
#ifdef __GNUC__
                        __attribute__ ((format (printf, 5, 6)))
#endif
    ;

#define urj_log(lvl, ...) \
        do { \
            if ((lvl) >= urj_log_state.level) \
                urj_do_log (lvl, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } while (0)

/**
 * Print warning unless logging level is > URJ_LOG_LEVEL_WARNING
 *
 * @param e urj_error_t value
 * @param ... consists of a printf argument set. It needs to start with a
 *      const char *fmt, followed by arguments used by fmt.
 */
#define urj_warning(...) urj_log (URJ_LOG_LEVEL_WARNING, __VA_ARGS__)

/**
 * Print the error set by urj_error_set and call urj_error_reset.
 */
void urj_log_error_describe (urj_log_level_t level);

/**
 * Convert the named level into the corresponding urj_log_level_t.
 *
 * @param slevel the string to translate
 *
 * @return log level on success; -1 on error
 */
urj_log_level_t urj_string_log_level (const char *slevel);

/**
 * Convert the urj_log_level_t into a string.
 *
 * @param level the level to translate
 *
 * @return the name of the log level on success; "unknown" on error
 */
const char *urj_log_level_string (urj_log_level_t level);

#endif /* URJ_LOG_H */
