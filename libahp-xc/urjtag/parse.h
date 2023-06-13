/*
 * $Id$
 *
 * Copyright (C) 2003 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2003.
 *
 */

#ifndef URJ_PARSE_H
#define URJ_PARSE_H

#include <stdio.h>

#include "types.h"

/**
 * Turn a string into a bunch of tokens (space delimited).  Returns the
 * number of tokens via token_cnt.  Memory is all allocated as needed, so
 * tokens should be passed to urj_tokens_free() when finished.
 *
 * @return
 *      URJ_STATUS_OK on success
 *      URJ_STATUS_ERROR on error
 */
int urj_tokenize_line (const char *line, char ***tokens, size_t *token_cnt);

/**
 * Free memory allocated for the tokens by urj_tokenize_line().
 */
void urj_tokens_free (char **tokens);

/**
 * Take care of tokenizing the line before sending to urj_cmd_run().
 *
 * @return
 *      URJ_STATUS_OK on success
 *      URJ_STATUS_ERROR on error
 *      URJ_STATUS_QUIT on quit command
 */
int urj_parse_line (urj_chain_t *chain, const char *line);

/**
 * Run each line in the specified stream through urj_parse_line().
 *
 * @return
 *      URJ_STATUS_OK on success
 *      URJ_STATUS_ERROR on error
 *      URJ_STATUS_QUIT on quit command
 */
int urj_parse_stream (urj_chain_t *chain, FILE *f);

/**
 * Open the specified file and run through urj_parse_stream().
 *
 * @return
 *      URJ_STATUS_OK on success
 *      URJ_STATUS_ERROR on error
 *      URJ_STATUS_QUIT on quit command
 */
int urj_parse_file (urj_chain_t *chain, const char *filename);

/**
 * Include a file. Autodetects whether it is a bsdl file or a UrJTAG command
 * shell script.
 *
 * @param filename if begins with a slash, or dots followed by a slash, ignore
 *      the search path
 * @param ignore_path ignore the search path anyway
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_parse_include (urj_chain_t *chain, const char *filename,
                       int ignore_path);

#endif /* URJ_PARSE_H */

