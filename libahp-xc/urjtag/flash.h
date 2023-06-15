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

#ifndef URJ_FLASH_H
#define URJ_FLASH_H

#include <stdio.h>
#include <stdint.h>

#include "types.h"

typedef struct URJ_FLASH_CFI_ARRAY urj_flash_cfi_array_t;

typedef int (*urj_flash_detect_func_t) (urj_bus_t *bus, uint32_t adr,
                                        urj_flash_cfi_array_t **cfi_array);

typedef struct
{
    const char *name;
    const char *description;
    unsigned int bus_width;     /* 1 for 8 bits, 2 for 16 bits, 4 for 32 bits, etc. */
    /** @return 1 if autodetected, 0 otherwise */
    int (*autodetect) (urj_flash_cfi_array_t *cfi_array);
    void (*print_info) (urj_log_level_t ll, urj_flash_cfi_array_t *cfi_array);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*erase_block) (urj_flash_cfi_array_t *cfi_array, uint32_t adr);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*lock_block) (urj_flash_cfi_array_t *cfi_array, uint32_t adr);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*unlock_block) (urj_flash_cfi_array_t *cfi_array, uint32_t adr);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*program) (urj_flash_cfi_array_t *cfi_array, uint32_t adr,
                    uint32_t *buffer, int count);
    void (*readarray) (urj_flash_cfi_array_t *cfi_array);
}
urj_flash_driver_t;

extern const urj_flash_driver_t * const urj_flash_flash_drivers[];

/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_flash_detectflash (urj_log_level_t ll, urj_bus_t *bus, uint32_t adr);
void urj_flash_cleanup (void);

/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_flashmem (urj_bus_t *bus, FILE *f, uint32_t addr, int);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_flashmsbin (urj_bus_t *bus, FILE *f, int);

/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_flasherase (urj_bus_t *bus, uint32_t addr, uint32_t number);

/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
int urj_flashlock (urj_bus_t *bus, uint32_t addr, uint32_t number, int unlock);

#endif /* URJ_FLASH_H */
