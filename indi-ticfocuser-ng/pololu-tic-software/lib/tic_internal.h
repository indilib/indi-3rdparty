#pragma once

#include <tic.h>
#include <config.h>

#include <libusbp.h>
#include <yaml.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _MSC_VER
#define TIC_PRINTF(f, a)
#else
#define TIC_PRINTF(f, a) __attribute__((format (printf, f, a)))
#endif

// A setup packet bRequest value from USB 2.0 Table 9-4
#define USB_REQUEST_GET_DESCRIPTOR 6

// A descriptor type from USB 2.0 Table 9-5
#define USB_DESCRIPTOR_TYPE_STRING 3


// Internal string manipulation library.

typedef struct tic_string
{
  char * data;
  size_t capacity;
  size_t length;
} tic_string;

void tic_string_setup(tic_string *);
void tic_string_setup_dummy(tic_string *);
TIC_PRINTF(2, 3)
void tic_sprintf(tic_string *, const char * format, ...);

#define STRING_TO_INT_ERR_SMALL 1
#define STRING_TO_INT_ERR_LARGE 2
#define STRING_TO_INT_ERR_EMPTY 3
#define STRING_TO_INT_ERR_INVALID 4

uint8_t tic_string_to_i64(const char *, int64_t *);


// Internal name lookup library.

typedef struct tic_name
{
  const char * name;
  uint32_t code;
} tic_name;

bool tic_name_to_code(const tic_name * table, const char * name, uint32_t * code);
bool tic_code_to_name(const tic_name * table, uint32_t code, const char ** name);

extern const tic_name tic_bool_names[];
extern const tic_name tic_product_names_short[];
extern const tic_name tic_step_mode_names[];
extern const tic_name tic_control_mode_names[];
extern const tic_name tic_response_names[];
extern const tic_name tic_scaling_degree_names[];
extern const tic_name tic_pin_func_names[];
extern const tic_name tic_agc_mode_names[];
extern const tic_name tic_agc_bottom_current_limit_names[];
extern const tic_name tic_agc_current_boost_steps_names[];
extern const tic_name tic_agc_frequency_limit_names[];
extern const tic_name tic_hp_decmod_names_snake[];
extern const tic_name tic_hp_decmod_names_ui[];
extern const tic_name tic_hp_driver_error_names_ui[];

// Intenral variables functions.

void tic_variables_set_from_device(tic_variables *, const uint8_t * buffer);


// Internal settings conversion functions.

uint32_t tic_baud_rate_from_brg(uint16_t brg);
uint16_t tic_baud_rate_to_brg(uint32_t baud_rate);


// Internal tic_device functions.

const libusbp_generic_interface *
tic_device_get_generic_interface(const tic_device * device);


// Internal tic_handle functions.

tic_error * tic_set_setting_segment(tic_handle * handle,
  uint8_t address, size_t length, const uint8_t * input);

tic_error * tic_get_setting_segment(tic_handle * handle,
  uint8_t address, size_t length, uint8_t * output);

tic_error * tic_get_variable_segment(tic_handle * handle,
  size_t index, size_t length, uint8_t * buf,
  bool clear_errors_occurred);


// Error creation functions.

tic_error * tic_error_add_code(tic_error * error, uint32_t code);

TIC_PRINTF(2, 3)
tic_error * tic_error_add(tic_error * error, const char * format, ...);

TIC_PRINTF(1, 2)
tic_error * tic_error_create(const char * format, ...);

tic_error * tic_usb_error(libusbp_error *);

extern tic_error tic_error_no_memory;


// Static helper functions

static inline uint32_t read_u32(const uint8_t * p)
{
  return p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
}

static inline int32_t read_i32(const uint8_t * p)
{
  return (int32_t)read_u32(p);
}

static inline uint32_t read_u16(const uint8_t * p)
{
  return p[0] + (p[1] << 8);
}


// Hidden settings, all of which are unimplemented in the firmware.

/// Sets the low VIN shutoff timeout.  This determines how long, in milliseconds,
/// it takes for a low reading on VIN to be interpreted as an error.
TIC_API
void tic_settings_set_low_vin_timeout(tic_settings *, uint16_t);

/// Gets the low VIN shutoff timeout setting described in
/// tic_settings_set_vin_shutoff_timeout().
TIC_API
uint16_t tic_settings_get_low_vin_timeout(const tic_settings *);

/// Sets the low VIN shutoff voltage.  This determines how low, in millivolts,
/// the VIN reading has to be for the controller to change from considering it
/// to be adequate to considering it to be too low.
TIC_API
void tic_settings_set_low_vin_shutoff_voltage(tic_settings *, uint16_t);

/// Gets the low VIN shutoff voltage setting described in
/// tic_settings_set_low_vin_shutoff_voltage().
TIC_API
uint16_t tic_settings_get_low_vin_shutoff_voltage(const tic_settings *);

/// Sets the Low VIN startup voltage.  This determines how high, in millivolts,
/// the VIN reading has to be for the controller to change from considering it
/// to be too low to considering it to be adequate.
TIC_API
void tic_settings_set_low_vin_startup_voltage(tic_settings *, uint16_t);

/// Gets the low VIN startup voltage setting described in
/// tic_settings_set_low_vin_startup_voltage().
TIC_API
uint16_t tic_settings_get_low_vin_startup_voltage(const tic_settings *);

/// Sets the High VIN shutoff voltage.  This determines how high, in millivolts,
/// the VIN reading has to be for the controller to change from considering it
/// to be adequate to considering it to be too high.
TIC_API
void tic_settings_set_high_vin_shutoff_voltage(tic_settings *, uint16_t);

/// Gets the High VIN shutoff voltage setting described in
/// tic_settings_set_high_vin_shutoff_voltage().
TIC_API
uint16_t tic_settings_get_high_vin_shutoff_voltage(const tic_settings *);

/// Sets the RC max pulse period.  This is the maximum allowed time between
/// consecutive RC pulse rising edges, in milliseconds.
TIC_API
void tic_settings_set_rc_max_pulse_period(tic_settings *, uint16_t);

/// Gets the RC max pulse period setting described in
/// tic_settings_set_rc_max_pulse_period().
TIC_API
uint16_t tic_settings_get_rc_max_pulse_period(const tic_settings *);

/// Sets the RC bad signal timeout.  This is the maximum time between valid RC
/// pulses that we use to update the motor, in milliseconds.
TIC_API
void tic_settings_set_rc_bad_signal_timeout(tic_settings *, uint16_t);

/// Gets the RC bad signal timeout setting described in
/// tic_settings_set_rc_bad_signal_timeout.
TIC_API
uint16_t tic_settings_get_rc_bad_signal_timeout(const tic_settings *);

/// Sets the RC consecutive good pulses setting.  This is the number of
/// consecutive good pulses that must be received before the controller starts
/// using good pulses to control the motor.  A value of 2 means that after 2
/// good pulses in a row are received, the third one will be used to control the
/// motor.  A value of 0 means that every good pulse results in an update of the
/// motor.
TIC_API
void tic_settings_set_rc_consecutive_good_pulses(tic_settings *, uint8_t);

/// Gets the RC consecutive good pulses setting described in
/// tic_settings_set_rc_consecutive_good_pulses().
TIC_API
uint8_t tic_settings_get_rc_consecutive_good_pulses(const tic_settings *);

/// Sets the input error minimum parameter.  In analog or RC control
/// mode, values below this will cause an error.
TIC_API
void tic_settings_set_input_error_min(tic_settings *, uint16_t);

/// Gets the input error minimum parameter.  See
/// tic_settings_set_error_min().
TIC_API
uint16_t tic_settings_get_input_error_min(const tic_settings *);

/// Sets the input error maximum parameter.  In analog or RC control
/// mode, values above this will cause an error.
TIC_API
void tic_settings_set_input_error_max(tic_settings *, uint16_t);

/// Gets the input error maximum parameter.  See
/// tic_settings_set_error_max().
TIC_API
uint16_t tic_settings_get_input_error_max(const tic_settings *);


// More internal helpers for settings.

typedef struct tic_settings_segments
{
  uint8_t general_offset, general_size;
  uint8_t product_specific_offset, product_specific_size;
} tic_settings_segments;

tic_settings_segments tic_get_settings_segments(uint8_t product);

uint32_t tic_settings_get_hp_toff_ns(const tic_settings *);
bool tic_settings_hp_gate_charge_ok(const tic_settings *);
