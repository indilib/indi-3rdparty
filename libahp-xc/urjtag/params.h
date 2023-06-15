/*
 * $Id$
 *
 * Parameter list, in the vein of X parameter passing
 * Copyright (C) 2009 VU Amsterdam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the ETC s.r.o. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Written by Rutger Hofman <rutger at cs dot vu dot nl>, 2009
 *
 */

#ifndef URJ_PARAMS_H
#define URJ_PARAMS_H

#include <stddef.h>

#include "types.h"

/**
 * Supported parameter types
 */
typedef enum URJ_PARAM_TYPE_T
{
    URJ_PARAM_TYPE_LU,
    URJ_PARAM_TYPE_STRING,
    URJ_PARAM_TYPE_BOOL,
}
urj_param_type_t;

/**
 * Parameter as assembled for passing to parameter-taking functions
 */
typedef struct URJ_PARAM
{
    urj_param_type_t        type;
    int                     key;
    union {
        long unsigned       lu;
        const char         *string;
        int                 enabled;    /**< bool */
    } value;
}
urj_param_t;

/**
 * Description of a parameter, as recognized by a module
 */
typedef struct URJ_PARAM_DESCR
{
    int                 key;            /**< key */
    urj_param_type_t    type;           /**< type */
    const char         *string;         /**< human-friendly form */
}
urj_param_descr_t;

/**
 * Type for a module to export its recognized parameters
 */
typedef struct URJ_PARAM_LIST
{
    const urj_param_descr_t *list;
    size_t                   n;
}
urj_param_list_t;

/** Initialise a parameter assembly line */
int urj_param_init (const urj_param_t ***bp);
int urj_param_init_list (const urj_param_t ***bp, char *params[],
                         const urj_param_list_t *param_list);
/** Clear the parameter assembly line */
int urj_param_clear (const urj_param_t ***bp);

/**
 * toString function for #urj_param_t. Is not reentrant. May return a
 * pointer to a static string.
 */
const char *urj_param_string(const urj_param_list_t *params,
                             const urj_param_t *p);

/**
 * Append a string-type argument to the current #urj_param_t assembly line.
 * Copies the pointer value, does no strdup(val).
 */
int urj_param_push_string (const urj_param_t ***bp, int key, const char *val);
/**
 * Append a ulong-type argument to the current #urj_param_t assembly line.
 */
int urj_param_push_lu (const urj_param_t ***bp, int key, long unsigned val);
/**
 * Append a bool-type argument to the current #urj_param_t assembly line.
 */
int urj_param_push_bool (const urj_param_t ***bp, int key, int val);
/**
 * Transform a string representation of a param "key=value" into a #urj_param_t
 * representation, and append it to the current #urj_param_t assembly line.
 */
int urj_param_push (const urj_param_list_t *params, const urj_param_t ***bp,
                    const char *p);
/**
 * Utility function to count the number of items in a #urj_param_t assembly
 * line
 */
size_t urj_param_num (const urj_param_t *params[]);

#endif /* URJ_BUS_DRIVER_BRUX_BUS_H */
