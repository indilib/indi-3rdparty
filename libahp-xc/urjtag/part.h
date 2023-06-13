/*
 * $Id$
 *
 * Copyright (C) 2002 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2002.
 *
 */

#ifndef URJ_PART_H
#define URJ_PART_H

#include "types.h"

#define URJ_PART_MANUFACTURER_MAXLEN    25
#define URJ_PART_PART_MAXLEN            20
#define URJ_PART_STEPPING_MAXLEN         8

struct URJ_PART_PARAMS
{
    void (*free) (void *);
    void (*wait_ready) (void *);
    void *data;
};

struct URJ_PART
{
    urj_tap_register_t *id;
    char *alias;                /* djf refdes */
    char manufacturer_name[URJ_PART_MANUFACTURER_MAXLEN + 1];
    char part_name[URJ_PART_PART_MAXLEN + 1];
    char stepping[URJ_PART_STEPPING_MAXLEN + 1];
    urj_part_signal_t *signals;
    urj_part_salias_t *saliases;
    int instruction_length;
    urj_part_instruction_t *instructions;
    urj_part_instruction_t *active_instruction;
    urj_data_register_t *data_registers;
    int boundary_length;
    urj_bsbit_t **bsbits;
    urj_part_params_t *params;
};

urj_part_t *urj_part_alloc (const urj_tap_register_t *id);
void urj_part_free (urj_part_t *p);
/* @return instruction pointer on success; NULL if not found but does not set
 * urj_error; NULL on error */
urj_part_instruction_t *urj_part_find_instruction (urj_part_t *p,
                                                   const char *iname);
/* @return data register pointer on success; NULL if not found but does not set
 * urj_error; NULL on error */
urj_data_register_t *urj_part_find_data_register (urj_part_t *p,
                                                  const char *drname);
/* @return signal pointer on success; NULL if not found but does not set
 * urj_error; NULL on error */
urj_part_signal_t *urj_part_find_signal (urj_part_t *p,
                                         const char *signalname);
void urj_part_set_instruction (urj_part_t *p, const char *iname);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_part_set_signal (urj_part_t *p, urj_part_signal_t *s, int out, int val);
#define urj_part_set_signal_high(p, s)  urj_part_set_signal ((p), (s), 1, 1)
#define urj_part_set_signal_low(p, s)   urj_part_set_signal ((p), (s), 1, 0)
#define urj_part_set_signal_input(p, s) urj_part_set_signal ((p), (s), 0, 0)

/** @return -1 on error; signal number >= 0 for success */
int urj_part_get_signal (urj_part_t *p, const urj_part_signal_t *s);
/* @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_part_print (urj_log_level_t ll, urj_part_t *p);
/**
 * Set the length of the instructions of a part
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error
 */
int urj_part_instruction_length_set (urj_part_t *part, int length);
/**
 * Create a new instruction for a part.
 * @param part
 * @param instruction name for the new instruction
 * @param code string that contains the bit pattern for the default instruction
 * @param data_register default data register for instruction (e.g. BR)
 */
urj_part_instruction_t *urj_part_instruction_define (urj_part_t *part,
                                                     const char *instruction,
                                                     const char *code,
                                                     const char *data_register);

typedef void (*urj_part_init_func_t) (urj_part_t *);

struct URJ_PART_INIT
{
    char part[URJ_PART_PART_MAXLEN + 1];
    urj_part_init_func_t init;
    urj_part_init_t *next;
};

/* List of registered part initializers.  */
extern urj_part_init_t *urj_part_inits;

void urj_part_init_register (char *part, urj_part_init_func_t init);
urj_part_init_func_t urj_part_find_init (char *part);

/**
 * parts
 */

struct URJ_PARTS
{
    int len;
    urj_part_t **parts;
};

urj_parts_t *urj_part_parts_alloc (void);
void urj_part_parts_free (urj_parts_t *ps);
/* @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_part_parts_add_part (urj_parts_t *ps, urj_part_t *p);
/* @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_part_parts_set_instruction (urj_parts_t *ps, const char *iname);
/* @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_part_parts_print (urj_log_level_t ll, urj_parts_t *ps, int active_part);

#endif /* URJ_PART_H */
