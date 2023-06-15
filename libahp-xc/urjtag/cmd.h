/*
 * $Id$
 *
 * Copyright (C) 2003 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2003.
 *
 */

#ifndef URJ_CMD_H
#define URJ_CMD_H

#include "types.h"

/**
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error. Consult
 *      urj_error for error details. Syntax errors in the input params are
 *      handled in the same way, urj_error is set to #URJ_ERROR_SYNTAX.
 */
int urj_cmd_run (urj_chain_t *chain, char *params[]);

/**
 * Attempt completion of part of a command string
 *
 * @param chain chain to possibly use for some completions
 * @param line  full (incomplete) command line
 * @param point current cursor position in the line
 *
 * @return malloc'ed array of strings. The caller is responsible for freeing
 *      all of them, and the array itself. NULL for malloc failure or end of
 *      possible completions.
 */
char **urj_cmd_complete (urj_chain_t *chain, const char *line, int point);

/**
 * Tests if chain has a cable pointer
 *
 * @return URJ_STATUS_OK if success; URJ_STATUS_FAIL on error or failure
 */
int urj_cmd_test_cable (urj_chain_t *chain);

/**
 * Count the number of parameters in this NULL-terminated list
 *
 * @param params pointer to array of null-terminated strings
 *
 * @return number of parameter strings; 0 for none.
 */
int urj_cmd_params (char * const params[]);

/**
 * Parse parameter as a long unsigned
 *
 * @param s string containing textual representation of a number
 * @param i pointer to variable in which to store the resulting number
 *
 * @return URJ_STATUS_OK on success, URJ_STATUS_FAIL on error
 */
int urj_cmd_get_number (const char *s, long unsigned *i);

#endif /* URJ_CMD_H */
