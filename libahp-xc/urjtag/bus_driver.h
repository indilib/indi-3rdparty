/*
 * $Id$
 *
 * Bus driver interface
 * Copyright (C) 2002, 2003 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2002, 2003.
 *
 */

#ifndef URJ_BUS_DRIVER_BRUX_BUS_H
#define URJ_BUS_DRIVER_BRUX_BUS_H

#include <stdint.h>

#include "types.h"
#include "params.h"

typedef struct
{
    const char *description;
    uint32_t start;
    uint64_t length;
    unsigned int width;
}
urj_bus_area_t;

typedef enum URJ_BUS_PARAM_KEY
{
    URJ_BUS_PARAM_KEY_MUX,      /* bool                         mpc5200 */
    /* avr32: mode = OCD | HSBC | HSBU | x8 | x16 | x32         avr32 */
    URJ_BUS_PARAM_KEY_OCD,      /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_HSBC,     /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_HSBU,     /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_X8,       /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_X16,      /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_X32,      /* bool                         avr32 */
    URJ_BUS_PARAM_KEY_WIDTH,    /* 0=auto 8 16 32 64 */
                                /* aliased as x8 x16 x32 bool   avr32 */
                                /* 8 32 64                      mpc824 */
                                /* aliased as AMODE             prototype */
    URJ_BUS_PARAM_KEY_OPCODE,   /* string                       fjmem */
    URJ_BUS_PARAM_KEY_LEN,      /* ulong                        fjmem */
    URJ_BUS_PARAM_KEY_AMODE,    /* alias for WIDTH: 0=auto 8 16 32  prototype */
    URJ_BUS_PARAM_KEY_ALSB,     /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_AMSB,     /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_DLSB,     /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_DMSB,     /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_CS,       /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_NCS,      /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_OE,       /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_NOE,      /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_WE,       /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_NWE,      /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_WP,       /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_NWP,      /* string (= signal name)       prototype */
    URJ_BUS_PARAM_KEY_REVBITS,  /* bool                         mpc824 */
    URJ_BUS_PARAM_KEY_HELP,     /* bool                         mpc824 */
    URJ_BUS_PARAM_KEY_DBGaDDR,  /* bool                         mpc824 */
    URJ_BUS_PARAM_KEY_DBGdATA,  /* bool                         mpc824 */
    URJ_BUS_PARAM_KEY_HWAIT,    /* string (= signal name)       blackfin */
}
urj_bus_param_key_t;

typedef enum URJ_BUS_TYPE
{
    URJ_BUS_TYPE_PARALLEL,
    URJ_BUS_TYPE_SPI,
    URJ_BUS_TYPE_I2C,
}
urj_bus_type_t;

struct URJ_BUS_DRIVER
{
    const char *name;
    const char *description;
    urj_bus_t *(*new_bus) (urj_chain_t *chain,
                           const urj_bus_driver_t *driver,
                           const urj_param_t *cmd_params[]);
    void (*free_bus) (urj_bus_t *bus);
    void (*printinfo) (urj_log_level_t ll, urj_bus_t *bus);
    void (*prepare) (urj_bus_t *bus);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*area) (urj_bus_t *bus, uint32_t adr, urj_bus_area_t *area);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on error */
    int (*read_start) (urj_bus_t *bus, uint32_t adr);
    /* @@@@ RFHH need to return status */
    uint32_t (*read_next) (urj_bus_t *bus, uint32_t adr);
    /* @@@@ RFHH need to return status */
    uint32_t (*read_end) (urj_bus_t *bus);
    /* @@@@ RFHH need to return status */
    uint32_t (*read) (urj_bus_t *bus, uint32_t adr);
    /* @@@@ RFHH need to return status */
    int (*write_start) (urj_bus_t *bus, uint32_t adr);
    void (*write) (urj_bus_t *bus, uint32_t adr, uint32_t data);
    int (*init) (urj_bus_t *bus);
    int (*enable) (urj_bus_t *bus);
    int (*disable) (urj_bus_t *bus);
    urj_bus_type_t bus_type;
};

struct URJ_BUS
{
    urj_chain_t *chain;
    urj_part_t *part;
    void *params;
    int initialized;
    int enabled;
    const urj_bus_driver_t *driver;
};


#define URJ_BUS_PRINTINFO(ll,bus)       (bus)->driver->printinfo(ll,bus)
#define URJ_BUS_PREPARE(bus)            (bus)->driver->prepare(bus)
#define URJ_BUS_AREA(bus,adr,a)         (bus)->driver->area(bus,adr,a)
#define URJ_BUS_READ_START(bus,adr)     (bus)->driver->read_start(bus,adr)
#define URJ_BUS_READ_NEXT(bus,adr)      (bus)->driver->read_next(bus,adr)
#define URJ_BUS_READ_END(bus)           (bus)->driver->read_end(bus)
#define URJ_BUS_READ(bus,adr)           (bus)->driver->read(bus,adr)
#define URJ_BUS_WRITE_START(bus,adr)    (bus)->driver->write_start(bus,adr)
#define URJ_BUS_WRITE(bus,adr,data)     (bus)->driver->write(bus,adr,data)
#define URJ_BUS_FREE(bus)               (bus)->driver->free_bus(bus)
#define URJ_BUS_INIT(bus)               (bus)->driver->init(bus)
#define URJ_BUS_ENABLE(bus)             (bus)->driver->enable(bus)
#define URJ_BUS_DISABLE(bus)            (bus)->driver->disable(bus)
#define URJ_BUS_TYPE(bus)               (bus)->driver->bus_type

/**
 * API function to init a bus
 */
urj_bus_t *urj_bus_init_bus (urj_chain_t *chain,
                             const urj_bus_driver_t *bus_driver,
                             const urj_param_t *param[]);

/** The list of recognized parameters */
extern const urj_param_list_t urj_bus_param_list;

#endif /* URJ_BUS_DRIVER_BRUX_BUS_H */
