/*
 * $Id$
 *
 * Cable driver interface
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

#ifndef URJ_CABLE_H
#define URJ_CABLE_H

#include <stdint.h>

#include "types.h"
#include "params.h"

#include "parport.h"
#include "pod.h"

typedef struct URJ_CABLE_DRIVER urj_cable_driver_t;

typedef enum URJ_CABLE_FLUSH_AMOUNT
{
    URJ_TAP_CABLE_OPTIONALLY,
    URJ_TAP_CABLE_TO_OUTPUT,
    URJ_TAP_CABLE_COMPLETELY
}
urj_cable_flush_amount_t;

typedef enum URJ_CABLE_DEVICE_TYPE
{
    URJ_CABLE_DEVICE_PARPORT,
    URJ_CABLE_DEVICE_USB,
    URJ_CABLE_DEVICE_OTHER,
}
urj_cable_device_type_t;

typedef enum URJ_CABLE_PARAM_KEY
{
    URJ_CABLE_PARAM_KEY_PID,            /* lu           generic_usbconn */
    URJ_CABLE_PARAM_KEY_VID,            /* lu           generic_usbconn */
    URJ_CABLE_PARAM_KEY_DESC,           /* string       generic_usbconn */
    URJ_CABLE_PARAM_KEY_DRIVER,         /* string       generic_usbconn */
    URJ_CABLE_PARAM_KEY_BITMAP,         /* string       wiggler */
    URJ_CABLE_PARAM_KEY_TDI,            /* lu           gpio used as TDI */
    URJ_CABLE_PARAM_KEY_TDO,            /* lu           gpio used as TDO */
    URJ_CABLE_PARAM_KEY_TMS,            /* lu           gpio used as TMS */
    URJ_CABLE_PARAM_KEY_TCK,            /* lu           gpio used as TCK */
    URJ_CABLE_PARAM_KEY_INTERFACE,      /* lu           ftdi */
    URJ_CABLE_PARAM_KEY_FIRMWARE,       /* string       ice100 */
    URJ_CABLE_PARAM_KEY_INDEX,          /* lu           ftdi */
    URJ_CABLE_PARAM_KEY_TRST,           /* lu           ft4232_generic */
    URJ_CABLE_PARAM_KEY_RESET,          /* lu           ft4232_generic */
}
urj_cable_param_key_t;

/* Random cable-specific quirks; a bitfield */
#define URJ_CABLE_QUIRK_ONESHOT 0x1

struct URJ_CABLE_DRIVER
{
    const char *name;
    const char *description;
    /** tag for the following union */
    urj_cable_device_type_t device_type;
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
    union {
        int (*parport) (urj_cable_t *cable, urj_cable_parport_devtype_t devtype,
                        const char *devname, const urj_param_t *params[]);
        int (*usb) (urj_cable_t *cable, const urj_param_t *params[]);
        int (*other) (urj_cable_t *cable, const urj_param_t *params[]);
    } connect;
    void (*disconnect) (urj_cable_t *cable);
    void (*cable_free) (urj_cable_t *cable);
    /** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
    int (*init) (urj_cable_t *);
    void (*done) (urj_cable_t *);
    void (*set_frequency) (urj_cable_t *, uint32_t freq);
    void (*clock) (urj_cable_t *, int, int, int);
    /** @return 0 or 1 on success; -1 on failure */
    int (*get_tdo) (urj_cable_t *);
    /** @return nonnegative number, or the number of transferred bits on
     * success; -1 on failure */
    int (*transfer) (urj_cable_t *, int, const char *, char *);
    /** @return 0 or 1 on success; -1 on failure */
    int (*set_signal) (urj_cable_t *, int, int);
    /** @return 0 or 1 on success; -1 on failure */
    int (*get_signal) (urj_cable_t *, urj_pod_sigsel_t);
    void (*flush) (urj_cable_t *, urj_cable_flush_amount_t);
    void (*help) (urj_log_level_t ll, const char *);
    /* A bitfield of quirks */
    uint32_t quirks;
};

typedef struct URJ_CABLE_QUEUE urj_cable_queue_t;

struct URJ_CABLE_QUEUE
{
    enum
    {
        URJ_TAP_CABLE_CLOCK,
        URJ_TAP_CABLE_CLOCK_COMPACT,
        URJ_TAP_CABLE_GET_TDO,
        URJ_TAP_CABLE_TRANSFER,
        URJ_TAP_CABLE_SET_SIGNAL,
        URJ_TAP_CABLE_GET_SIGNAL
    } action;
    union
    {
        struct
        {
            int tms;
            int tdi;
            int n;
        } clock;
        struct
        {
            urj_pod_sigsel_t sig;
            int mask;
            int val;
        } value;
        struct
        {
            int len;
            char *in;
            char *out;
        } transfer;
        struct
        {
            int len;
            int res;
            char *out;
        } xferred;
    } arg;
};

typedef struct URJ_CABLE_QUEUE_INFO urj_cable_queue_info_t;

struct URJ_CABLE_QUEUE_INFO
{
    urj_cable_queue_t *data;
    int max_items;
    int num_items;
    int next_item;
    int next_free;
};

struct URJ_CABLE
{
    const urj_cable_driver_t *driver;
    union
    {
        urj_usbconn_t *usb;
        urj_parport_t *port;
        void *other;
    } link;
    void *params;
    urj_chain_t *chain;
    urj_cable_queue_info_t todo;
    urj_cable_queue_info_t done;
    uint32_t delay;
    uint32_t frequency;
};

void urj_tap_cable_free (urj_cable_t *cable);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
int urj_tap_cable_init (urj_cable_t *cable);
/** @return cable named by @cname; NULL on failure */
const urj_cable_driver_t *urj_tap_cable_find (const char *cname);
void urj_tap_cable_done (urj_cable_t *cable);
void urj_tap_cable_flush (urj_cable_t *cable,
                          urj_cable_flush_amount_t);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
void urj_tap_cable_clock (urj_cable_t *cable, int tms, int tdi, int n);
int urj_tap_cable_defer_clock (urj_cable_t *cable, int tms, int tdi, int n);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_cable_get_tdo (urj_cable_t *cable);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_cable_get_tdo_late (urj_cable_t *cable);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
int urj_tap_cable_defer_get_tdo (urj_cable_t *cable);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_cable_set_signal (urj_cable_t *cable, int mask, int val);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
int urj_tap_cable_defer_set_signal (urj_cable_t *cable, int mask, int val);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_cable_get_signal (urj_cable_t *cable, urj_pod_sigsel_t sig);
/** @return 0 or 1 on success; -1 on failure */
int urj_tap_cable_get_signal_late (urj_cable_t *cable, urj_pod_sigsel_t sig);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
int urj_tap_cable_defer_get_signal (urj_cable_t *cable, urj_pod_sigsel_t sig);
/** @return the number of transferred bits on success; -1 on failure */
int urj_tap_cable_transfer (urj_cable_t *cable, int len, char *in, char *out);
/** @return the number of transferred bits on success; -1 on failure */
int urj_tap_cable_transfer_late (urj_cable_t *cable, char *out);
/** @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure */
int urj_tap_cable_defer_transfer (urj_cable_t *cable, int len, char *in,
                                  char *out);

void urj_tap_cable_set_frequency (urj_cable_t *cable, uint32_t frequency);
uint32_t urj_tap_cable_get_frequency (urj_cable_t *cable);
void urj_tap_cable_wait (urj_cable_t *cable);
void urj_tap_cable_purge_queue (urj_cable_queue_info_t *q, int io);
/** @return queue item number on success; -1 on failure */
int urj_tap_cable_add_queue_item (urj_cable_t *cable,
                                  urj_cable_queue_info_t *q);
/** @return queue item number on success; -1 on failure */
int urj_tap_cable_get_queue_item (urj_cable_t *cable,
                                  urj_cable_queue_info_t *q);

/**
 * API function to connect to a parport cable
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure
 */
urj_cable_t *urj_tap_cable_parport_connect (urj_chain_t *chain,
                                            const urj_cable_driver_t *driver,
                                            urj_cable_parport_devtype_t devtype,
                                            const char *devname,
                                            const urj_param_t *params[]);
/**
 * API function to connect to a USB cable
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure
 */
urj_cable_t *urj_tap_cable_usb_connect (urj_chain_t *chain,
                                        const urj_cable_driver_t *driver,
                                        const urj_param_t *params[]);
int urj_tap_cable_usb_probe (char *params[]);

/**
 * API function to connect to a type-other cable
 *
 * @return URJ_STATUS_OK on success; URJ_STATUS_FAIL on failure
 */
urj_cable_t *urj_tap_cable_other_connect (urj_chain_t *chain,
                                          const urj_cable_driver_t *driver,
                                          const urj_param_t *params[]);

extern const urj_cable_driver_t * const urj_tap_cable_drivers[];

/** The list of recognized parameters */
extern const urj_param_list_t urj_cable_param_list;

#endif /* URJ_CABLE_H */
