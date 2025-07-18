// Copyright (C) Pololu Corporation.  See www.pololu.com for details.

#ifndef TICDEFS_H
#define TICDEFS_H

#include <cstdint>
#include <unistd.h>

// copied from pololu-tic-software/include/tic_protocol.h
// licensed on MIT license
#define TIC_VENDOR_ID 0x1FFB
#define TIC_PRODUCT_ID_T825 0x00B3
#define TIC_PRODUCT_ID_T834 0x00B5
#define TIC_PRODUCT_ID_T500 0x00BD
#define TIC_PRODUCT_ID_N825 0x00C3
#define TIC_PRODUCT_ID_T249 0x00C9
#define TIC_PRODUCT_ID_36V4 0x00CB

// copied from pololu-tic-software/lib/tic_internal.h
typedef struct tic_name
{
  const char * name;
  uint32_t code;
} tic_name;

// copied from pololu-tic-software/lib/tic_names.c
extern const tic_name tic_error_names_ui[];
// added
extern const size_t tic_error_names_ui_size;

// copied from pololu-tic-software/include/tic.h
const char * tic_look_up_operation_state_name_ui(uint8_t operation_state);
const char * tic_look_up_step_mode_name_ui(uint8_t step_mode);

#endif // TICDEFS_H
