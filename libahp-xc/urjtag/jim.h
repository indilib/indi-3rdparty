/*
 * $Id$
 *
 * Copyright (C) 2008 Kolja Waschk <kawk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Documentation used while writing this code:
 *
 * http://www.inaccessnetworks.com/projects/ianjtag/jtag-intro/jtag-intro.html
 *   "A Brief Introduction to the JTAG Boundary Scan Interface", Nick Patavalis
 *
 * http://www.xjtag.com/support-jtag/jtag-technical-guide.php
 *   "JTAG - A technical overview", XJTAG Ltd.
 *
 */

#ifndef URJ_JIM_H
#define URJ_JIM_H 1

#include <stdint.h>
#include <stddef.h>

typedef enum jim_tap_state
{
    URJ_JIM_RESET = 0,
    URJ_JIM_SELECT_DR = 0 + 1,
    URJ_JIM_CAPTURE_DR = 0 + 2,
    URJ_JIM_SHIFT_DR = 0 + 3,
    URJ_JIM_EXIT1_DR = 0 + 4,
    URJ_JIM_PAUSE_DR = 0 + 5,
    URJ_JIM_EXIT2_DR = 0 + 6,
    URJ_JIM_UPDATE_DR = 0 + 7,
    URJ_JIM_IDLE = 8,
    URJ_JIM_SELECT_IR = 8 + 1,
    URJ_JIM_CAPTURE_IR = 8 + 2,
    URJ_JIM_SHIFT_IR = 8 + 3,
    URJ_JIM_EXIT1_IR = 8 + 4,
    URJ_JIM_PAUSE_IR = 8 + 5,
    URJ_JIM_EXIT2_IR = 8 + 6,
    URJ_JIM_UPDATE_IR = 8 + 7,
}
urj_jim_tap_state_t;

typedef struct
{
    uint32_t *reg;
    int len;
}
urj_jim_shift_reg_t;

typedef struct URJ_JIM_DEVICE urj_jim_device_t;

struct URJ_JIM_DEVICE
{
    urj_jim_device_t *prev;

    urj_jim_tap_state_t tap_state;
    void (*tck_rise) (urj_jim_device_t *dev, int tms, int tdi,
                      uint8_t *shmem, size_t shmem_size);
    void (*tck_fall) (urj_jim_device_t *dev, uint8_t *shmem,
                      size_t shmem_size);
    void (*dev_free) (urj_jim_device_t *dev);
    void *state;
    int num_sregs;
    int current_dr;
    urj_jim_shift_reg_t *sreg;
    int tdo;
    int tdo_buffer;
};

typedef struct URJ_JIM_STATE
{
    int trst;
    uint8_t *shmem;
    size_t shmem_size;
    urj_jim_device_t *last_device_in_chain;
}
urj_jim_state_t;

typedef struct URJ_JIM_BUS_DEVICE urj_jim_bus_device_t;

struct URJ_JIM_BUS_DEVICE
{
    int width;                  /* bytes */
    int size;                   /* words (each <width> bytes) */
    void *state;                /* device-dependent */
    int (*init) (urj_jim_bus_device_t *x);
    uint32_t (*capture) (urj_jim_bus_device_t *x,
                         uint32_t address, uint32_t control,
                         uint8_t *shmem, size_t shmem_size);
    void (*update) (urj_jim_bus_device_t *x,
                    uint32_t address, uint32_t data, uint32_t control,
                    uint8_t *shmem, size_t shmem_size);
    void (*free) (urj_jim_bus_device_t *x);
};

typedef struct
{
    uint32_t offset;
    int adr_shift;
    int data_shift;
    urj_jim_bus_device_t *part;
}
urj_jim_attached_part_t;

void urj_jim_set_trst (urj_jim_state_t *s, int trst);
int urj_jim_get_trst (urj_jim_state_t *s);
int urj_jim_get_tdo (urj_jim_state_t *s);
void urj_jim_tck_rise (urj_jim_state_t *s, int tms, int tdi);
void urj_jim_tck_fall (urj_jim_state_t *s);
urj_jim_device_t *urj_jim_alloc_device (int num_sregs, const int reg_size[]);
urj_jim_state_t *urj_jim_init (void);
void urj_jim_free (urj_jim_state_t *s);
urj_jim_device_t *urj_jim_some_cpu (void);

#endif
