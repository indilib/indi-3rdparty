/*
 * $Id$
 *
 * Global opaque types that had better be predefined
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

#ifndef URJ_URJ_TYPES_H
#define URJ_URJ_TYPES_H

typedef struct URJ_BUS urj_bus_t;
typedef struct URJ_BUS_DRIVER urj_bus_driver_t;
typedef struct URJ_CHAIN urj_chain_t;
typedef struct URJ_CABLE urj_cable_t;
typedef struct URJ_USBCONN urj_usbconn_t;
typedef struct URJ_PARPORT urj_parport_t;
typedef struct URJ_PART urj_part_t;
typedef struct URJ_PARTS urj_parts_t;
typedef struct URJ_PART_SIGNAL urj_part_signal_t;
typedef struct URJ_PART_SALIAS urj_part_salias_t;
typedef struct URJ_PART_INSTRUCTION urj_part_instruction_t;
typedef struct URJ_PART_PARAMS urj_part_params_t;
typedef struct URJ_PART_INIT urj_part_init_t;
typedef struct URJ_DATA_REGISTER urj_data_register_t;
typedef struct URJ_BSBIT urj_bsbit_t;
typedef struct URJ_TAP_REGISTER urj_tap_register_t;

/**
 * Log levels
 */
typedef enum URJ_LOG_LEVEL
{
    URJ_LOG_LEVEL_ALL,          /**< every single bit as it is transmitted */
    URJ_LOG_LEVEL_COMM,         /**< low level communication details */
    URJ_LOG_LEVEL_DEBUG,        /**< more details of interest for developers */
    URJ_LOG_LEVEL_DETAIL,       /**< verbose output */
    URJ_LOG_LEVEL_NORMAL,       /**< just noteworthy info */
    URJ_LOG_LEVEL_WARNING,      /**< unmissable warnings */
    URJ_LOG_LEVEL_ERROR,        /**< only fatal errors */
    URJ_LOG_LEVEL_SILENT,       /**< suppress logging output */
}
urj_log_level_t;

#define URJ_STATUS_OK             0
#define URJ_STATUS_FAIL           1
#define URJ_STATUS_MUST_QUIT    (-2)

#endif /* URJ_URJ_TYPES_H */
