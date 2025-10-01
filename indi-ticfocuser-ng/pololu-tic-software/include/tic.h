// Copyright (C) Pololu Corporation.  See www.pololu.com for details.

/// \file tic.h
///
/// This file provides the C API for libpololu-tic, a library for configuring
/// and controlling the Pololu Tic USB Stepper Motor Controller over USB.
///
/// If you want to communicate with the Tic over USB, you will need these
/// functions:
///
/// - tic_list_connected_devices()
/// - tic_handle_open()
/// - tic_list_free()
/// - tic_handle_close()
/// - tic_device_free()
///
/// If you want to control the position or speed of a stepper motor over USB,
/// see these functions:
///
/// - tic_exit_safe_start()
/// - tic_set_target_speed()
/// - tic_get_target_position()
///
/// If you want to get the current status of the Tic, include analog and digital
/// readings from its pins, the current position of the stepper motor, and any
/// errors, see these functions:
///
/// - tic_get_variables()
/// - tic_variables_free()
///
/// If you want to change the settings of the Tic, see these functions:
///
/// - tic_get_settings()
/// - tic_set_settings()
/// - tic_restore_defaults()
/// - tic_reinitialize()
/// - tic_settings_free()
///
/// Most library functions return a tic_error pointer if an error happens.  To
/// avoid memory leaks, you must free the error when that happens.  You can use
/// these functions to get detailed error information and free the error:
///
/// - tic_error_get_message()
/// - tic_error_has_code()
/// - tic_error_free()
///
/// Many of the functions in this file refer to numeric constant macros defined
/// in tic_protocol.h.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "tic_protocol.h"

#define TIC_PRODUCT_T825 1
#define TIC_PRODUCT_T834 2
#define TIC_PRODUCT_T500 3
#define TIC_PRODUCT_N825 4
#define TIC_PRODUCT_T249 5
#define TIC_PRODUCT_36V4 6

// The maximum firmware major version supported by this library.
#define TIC_FIRMWARE_VERSION_MAJOR_MAX 1

#ifdef _WIN32
#  define TIC_DLL_EXPORT __declspec(dllexport)
#  define TIC_DLL_IMPORT __declspec(dllimport)
#else
#  define TIC_DLL_IMPORT __attribute__((visibility ("default")))
#  define TIC_DLL_EXPORT __attribute__((visibility ("default")))
#endif

#ifdef _MSC_VER
#define TIC_WARN_UNUSED _Check_return_
#else
#define TIC_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#ifdef TIC_STATIC
#  define TIC_API
#else
#  ifdef TIC_EXPORTS
#    define TIC_API TIC_DLL_EXPORT
#  else
#    define TIC_API TIC_DLL_IMPORT
#  endif
#endif

/// Certain functions in the library return a newly-created string and require
/// the caller to call this function to free the string.  Passing a NULL pointer
/// to this function is OK.  Do not free the same non-NULL string twice.
void tic_string_free(char *);


// Simple name lookup ///////////////////////////////////////////////////////////

/// Looks up a short code string without spaces representing the product.  The
/// product argument should be one of the TIC_PRODUCT_* macros, but if it is
/// not, this function returns an empty string.
TIC_API
const char * tic_look_up_product_name_short(uint8_t product);

/// Looks up a user-friendly string for the product name.  The product argument
/// should be one of the TIC_PRODUCT_* macros, but if it is not, this function
/// returns "(Unknown)".
TIC_API
const char * tic_look_up_product_name_ui(uint8_t product);

/// Looks up a user-friendly string corresponding to the specified error bit,
/// e.g. "Safe start violation".  The error argument should be of the form (1 <<
/// x) where x is one of the TIC_ERROR_* macros, but if it is not, this function
/// returns "(Unknown)".  The returned string will be valid indefinitely and
/// should not be freed.
TIC_API
const char * tic_look_up_error_name_ui(uint32_t error);

/// Note: This function is deprecated.  See tic_look_up_decay_mode_name().
///
/// Looks up a user-friendly string corresponding to the specified decay mode, e.g. "Mixed".
/// The decay_mode argument should be one of the TIC_DECAY_MODE_* macros, but if
/// it is not, this function returns "(Unknown)".  The returned string will
/// be valid indefinitely and should not be freed.
TIC_API __attribute__((deprecated))
const char * tic_look_up_decay_mode_name_ui(uint8_t decay_mode);

/// Loops up a user-friendly string corresponding to the specified input state,
/// e.g. "Not ready".  The input_state argument should be one of the
/// TIC_INPUT_STATE_* macros, but if it is not, this function returns
/// "(Unknown)".  The returned string will be valid indefinitely and should not
/// be freed.  See tic_variables_get_input_state().
TIC_API
const char * tic_look_up_input_state_name_ui(uint8_t input_state);

/// Looks up a user-friendly string corresponding to the specified device reset, e.g. "Stack
/// underflow".  The device_reset argument should be one of the TIC_RESET_*
/// macros, but if it is not, this function returns "(Unknown)".  The returned
/// string will be valid indefinitely and should not be freed.
TIC_API
const char * tic_look_up_device_reset_name_ui(uint8_t device_reset);

/// Looks up a user-friendly string corresponding to the specified operation
/// state, e.g. "holding".  The operation_state argument should be one of the
/// TIC_OPERATION_STATE_* macros, but if it is not, this function returns
/// "(Unknown)".  The returned string will be valid indefinitely and should not
/// be freed.
TIC_API
const char * tic_look_up_operation_state_name_ui(uint8_t operation_state);

/// Looks up a user-friendly string corresponding to the specified step mode,
/// e.g. "Full step" or "1/2 step".  The step_mode argument should be one of the
/// TIC_STEP_MODE_* macros, but if it is not, this functions returns
/// "(Unknown)".  The returned string will be valid indefinitely and should not
/// be freed.
TIC_API
const char * tic_look_up_step_mode_name_ui(uint8_t step_mode);

/// Looks up the string corresponding to the specified pin state, e.g. "Output
/// low".  The pin_state argument should be one of the TIC_PIN_STATE_* macros,
/// but if it is not, this functions returns "(Unknown)".  The returned string
/// will be valid indefinitely and should not be freed.
TIC_API
const char * tic_look_up_pin_state_name_ui(uint8_t pin_state);

/// Looks up the string corresponding to the specified planning mode,
/// e.g. "Target position".  The planning_mode argument should be one of the
/// TIC_PLANNING_MODE_* macros, but if it is not, this functions returns
/// "(Unknown)".  The returned string will be valid indefinitely and should not
/// be freed.
TIC_API
const char * tic_look_up_planning_mode_name_ui(uint8_t planning_mode);

/// Looks up the string corresponding to the specified motor driver error.
/// The argument should be one of the TIC_MOTOR_DRIVER_ERROR_* macros
/// (e.g. the return value of tic_variables_get_last_motor_driver_error()),
/// but if it is not, this function returns "(Unknown)".  The returned string
/// will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic T249.
TIC_API
const char * tic_look_up_motor_driver_error_name_ui(uint8_t error);

/// Looks up the string corresponding to the specified AGC mode.
/// The argument should be one of the TIC_AGC_MODE_* macros,
/// but if it is not, this function returns "(Unknown)".  The returned string
/// will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic T249.
TIC_API
const char * tic_look_up_agc_mode_name_ui(uint8_t mode);

/// Looks up the string corresponding to the specified AGC bottom current limit
/// setting.  The argument should be one of the TIC_AGC_BOTTOM_CURRENT_LIMIT_*
/// macros, but if it is not, this function returns "(Unknown)".  The returned
/// string will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic T249.
TIC_API
const char * tic_look_up_agc_bottom_current_limit_name_ui(uint8_t limit);

/// Looks up the string corresponding to the specified AGC current boost steps
/// setting.  The argument should be one of the TIC_AGC_CURRENT_BOOST_STEPS_*
/// macros, but if it is not, this function returns "(Unknown)".  The returned
/// string will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic T249.
TIC_API
const char * tic_look_up_agc_current_boost_steps_name_ui(uint8_t steps);

/// Looks up the string corresponding to the specified AGC frequency limit
/// setting.  The argument should be one of the TIC_AGC_CURRENT_BOOST_STEPS_*
/// macros, but if it is not, this function returns "(Unknown)".  The returned
/// string will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic T249.
TIC_API
const char * tic_look_up_agc_frequency_limit_name_ui(uint8_t limit);

/// Looks up a user-friendly string corresponding to the specified Tic 36v4
/// driver error bit, e.g. "Overtemperature".
///
/// The \p error argument should be bitwise-or combination of numbers of the
/// form (1 << x), where x is one of the TIC_HP_DRIVER_ERROR_* macros.
/// If it is not, this function returns "(Unknown)".
/// If the argument is 0, this function returns "None".
/// If multiple error bits are 1 and this function does not know a
/// name to describe the set of errors, it returns "(Multiple)".
///
/// The returned string will be valid indefinitely and
/// should not be freed.
///
/// This function is only useful for the Tic 36v4.
///
/// See also tic_variables_get_last_hp_driver_errors().
TIC_API
const char * tic_look_up_hp_driver_error_name_ui(uint8_t error);

/// Looks up a user-friendly string corresponding to the specified Tic 36v4
/// decay mode, e.g. "Mixed".
///
/// The \p mode argument should be one of the TIC_HP_DECMOD_*
/// macros, but if it is not, this function returns "(Unknown)".
/// The returned string will be valid indefinitely and should not be freed.
///
/// This function is only useful for the Tic 36v4.
const char * tic_look_up_hp_decmod_name_ui(uint8_t mode);


// Advanced name/code lookup ////////////////////////////////////////////////////

#define TIC_NAME_UI 1
#define TIC_NAME_SNAKE_CASE 2

/// Looks up a string corresponding to the specified decay mode.
///
/// The `decay_mode` argument should be one of the TIC_DECAY_MODE_* macros that is
/// valid for the specified product.
///
/// The `product` argument should be the TIC_PRODUCT_* macro of the product you
/// are using.  If you do not know which product you are using, specify 0 to get
/// generic names.
///
/// The `flags` argument should be an inclusive OR of some subset of the
/// following flags:
///
/// - `TIC_NAME_UI` to retrieve a UI-style name (e.g. "Mixed 50%").
/// - `TIC_NAME_SNAKE_CASE` to retrieve a snake_case name (e.g. "mixed50").
///
/// If no name style is specified, `TIC_NAME_UI` is assumed.
///
/// The `name` argument should be a pointer to a variable to receive the name
/// string, or NULL.  If it is not NULL, this function uses the pointer to
/// return a pointer to the name string to the caller.  The returned string is
/// valid indefinitely, and should not be freed.  The string will be "(Unknown)"
/// (UI-style) or "" (snake-case) if the function failed.
///
/// If this function succeeds, it returns true.  Otherwise, it returns false.
///
/// This function is only useful to the Tic T825, Tic N825, and Tic T834.
/// For the Tic 36v4, see tic_look_up_hp_decmod_name_ui().
///
/// See also tic_look_up_decay_mode_code().
TIC_API
bool tic_look_up_decay_mode_name(uint8_t decay_mode,
  uint8_t product, uint32_t flags, const char ** name);

/// Looks up the the decay mode corresponding to the specified input string.
///
/// The `name` argument should be a null-terminated string.
///
/// The `product` argument should be the TIC_PRODUCT_* macro of the product you
/// are using.  If you do not know which product you are using, specify 0 to
/// allow any decay mode name for any product.
///
/// The `flags` argument should be an inclusive OR of some subset of the
/// following flags:
///
/// - `TIC_NAME_UI` to allow UI-style names (e.g. "Mixed 50%").
/// - `TIC_NAME_SNAKE_CASE` to allow snake_case names (e.g. "mixed50").
///
/// The `code` argument should be a pointer to a variable to receive the code,
/// or NULL.  If it is not NULL, this function uses the pointer to return the
/// decay mode code to the caller.  If the function fails, the returned code
/// will be 0.
///
/// If this function succeeds, it returns true.  Otherwise, it returns false.
///
/// See also tic_look_up_decay_mode_name().
TIC_API
bool tic_look_up_decay_mode_code(const char * name,
  uint8_t product, uint32_t flags, uint8_t * code);


// tic_error ////////////////////////////////////////////////////////////////////

/// A ::tic_error object represents an error that occurred in the library.  Many
/// functions return a tic_error pointer.  The convention is that a NULL pointer
/// indicates success.  If the pointer is not NULL, the caller can use the
/// pointer to get information about the error, and then must free the error at
/// some point by calling tic_error_free.
typedef struct tic_error tic_error;

/// Each ::tic_error can have 0 or more error codes that give additional
/// information about the error that might help the caller take the right action
/// when the error occurs.  This enum defines which error codes are possible.
enum tic_error_code
{
  /// There were problems allocating memory.  A memory shortage might be the
  /// root cause of the error, or there might be another error that is masked by
  /// the memory problems.
  TIC_ERROR_MEMORY = 1,

  /// Access was denied.  A common cause of this error on Windows is that
  /// another application has a handle open to the same device.
  TIC_ERROR_ACCESS_DENIED = 2,

  /// The device took too long to respond to a request or transfer data.
  TIC_ERROR_TIMEOUT = 3,

  /// The error might have been caused by the device being disconnected, but it
  /// is possible it was caused by something else.
  TIC_ERROR_DEVICE_DISCONNECTED = 4,
};

/// Attempts to copy an error.  If you copy a NULL ::tic_error pointer, the
/// result will also be NULL.  If you copy a non-NULL ::tic_error pointer, the
/// result will be non-NULL, but if there are issues allocating memory, then the
/// copied error might have different properties than the original error, and it
/// will have the ::TIC_ERROR_MEMORY code.
///
/// It is the caller's responsibility to free the copied error.
TIC_API TIC_WARN_UNUSED
tic_error * tic_error_copy(const tic_error *);

/// Frees a returned error object.  Passing the NULL pointer to this function is
/// OK.  Do not free the same non-NULL error twice.
TIC_API
void tic_error_free(tic_error *);

/// Returns true if the error has specified error code.  The error codes are
/// listed in the ::tic_error_code enum.
TIC_API TIC_WARN_UNUSED
bool tic_error_has_code(const tic_error *, uint32_t code);

/// Returns an English-language ASCII-encoded string describing the error.  The
/// string consists of one or more complete sentences.
///
/// The returned pointer will be valid at least until the error is freed.
TIC_API TIC_WARN_UNUSED
const char * tic_error_get_message(const tic_error *);


// tic_settings ////////////////////////////////////////////////////////////////

/// Represents the settings for a Tic.  This object is just plain old data; it
/// does not have any pointers or handles for other resources.
typedef struct tic_settings tic_settings;

/// Creates a new settings object.
///
/// The new settings object will have no product specified and all settings set
/// to zero.  After creating the settings object, you would typically call
/// tic_settings_set_product() and then tic_settings_fill_with_defaults().
///
/// Then you would use setter and getter methods to work with the settings.  At
/// some point, you should call tic_settings_fix() to make sure the settings are
/// valid and provide warnings about what settings were invalid.
///
/// The settings parameter should be a non-null pointer to a tic_settings
/// pointer, which will receive a pointer to a new settings object if and only
/// if this function is successful.  The caller must free the settings later by
/// calling tic_settings_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_settings_create(tic_settings ** settings);

/// Copies a tic_settings object.  If this function is successful, the caller must
/// free the settings later by calling tic_settings_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_settings_copy(
  const tic_settings * source ,
  tic_settings ** dest);

/// Frees a tic_settings object.  It is OK to pass a NULL pointer to this
/// function.  Do not free the same non-NULL settings object twice.
TIC_API
void tic_settings_free(tic_settings *);

/// Fixes the settings to have defaults.  Before calling this, you should
/// specify what product the settings are for by calling
/// tic_settings_set_product().  If the product is not set to a valid non-zero
/// value, this function will do nothing.
TIC_API
void tic_settings_fill_with_defaults(tic_settings * settings);

/// Fixes the settings to be valid and consistent.
///
/// The warnings parameters is an optional pointer to pointer to a string.  If
/// you supply the warnings parameter, and this function is successful, this
/// function will allocate and return a string using the parameter.  The string
/// will describe what was fixed in the settings or be empty if nothing was
/// fixed.  Each distinct warning in the string will be a series of complete
/// English sentences that ends with a newline character.  The string must be
/// freed by the caller using tic_string_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_settings_fix(tic_settings *, char ** warnings);

/// Gets the settings as a YAML string, also known as a settings file.  If this
/// function is successful, the string must be freed by the caller using
/// tic_string_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_settings_to_string(const tic_settings *, char ** string);

/// Parses an YAML settings string, also known as a settings file, and returns
/// the corresponding settings object.  The settings returned might be invalid,
/// so it is recommended to call tic_settings_fix() to fix the settings and warn
/// the user.
///
/// The settings parameter should be a non-null pointer to a tic_settings
/// pointer, which will receive a pointer to a new settings object if and only
/// if this function is successful.  The caller must free the settings later by
/// calling tic_settings_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_settings_read_from_string(const char * string,
  tic_settings ** settings);

/// Sets the product, which specifies what Tic product these settings are for.
/// The value should be one of the TIC_PRODUCT_* macros.
TIC_API
void tic_settings_set_product(tic_settings *, uint8_t product);

/// Gets the product described in tic_settings_set_product().
TIC_API
uint8_t tic_settings_get_product(const tic_settings *);

/// Sets the firmware version associated with these settings.
///
/// The version number should be 0 to indicate the version is unknown, or
/// should come from tic_device_get_firmware_version().
///
/// Setting the firmware version allows tic_settings_fix() to disable features
/// that are not supported.
TIC_API
void tic_settings_set_firmware_version(tic_settings * settings,
  uint16_t version);

/// Gets the firmware version number described in
/// tic_settings_get_firmware_version().
TIC_API
uint16_t tic_settings_get_firmware_version(const tic_settings * settings);

/// Sets the control mode, which should be one of the TIC_CONTROL_MODE_* macros.
/// Silently obeys if the input is invalid so that you can see a warning later
/// when calling tic_settings_fix().
TIC_API
void tic_settings_set_control_mode(tic_settings *, uint8_t control_mode);

/// Returns the control mode described in tic_settings_set_control_mode().
TIC_API
uint8_t tic_settings_get_control_mode(const tic_settings *);

/// Sets the never sleep (ignore USB suspend) setting.  If true, prevents the
/// device from going into deep sleep mode in order to comply with suspend
/// current requirements of the USB specifications.
TIC_API
void tic_settings_set_never_sleep(tic_settings *, bool never_sleep);

/// Gets the Never sleep (ignore USB suspend) setting described in
/// tic_settings_set_never_sleep().
TIC_API
bool tic_settings_get_never_sleep(const tic_settings *);

/// Sets the disable safe start setting.  If true, it disables some extra
/// conditions that are required to let the device start moving the motor.
TIC_API
void tic_settings_set_disable_safe_start(tic_settings *, bool);

/// Gets the disable safe start setting described in
/// tic_settings_get_disable_safe_start().
TIC_API
bool tic_settings_get_disable_safe_start(const tic_settings *);

/// Sets the ignore ERR line high setting.  If true, allows the device to
/// operate even if the ERR line is being driven high by something.
TIC_API
void tic_settings_set_ignore_err_line_high(tic_settings *, bool);

/// Gets the ignore ERR line high setting described in
/// tic_settings_set_ignore_err_line_high().
TIC_API
bool tic_settings_get_ignore_err_line_high(const tic_settings *);

/// Sets the Auto clear driver error setting.  If true, the Tic will attempt to
/// automatically clear motor driver error if they happen.
TIC_API
void tic_settings_set_auto_clear_driver_error(tic_settings *, bool);

/// Gets the Auto clear driver error setting described in
/// tic_settings_set_auto_clear_driver_error().
TIC_API
bool tic_settings_get_auto_clear_driver_error(const tic_settings *);

/// Sets the Soft error response setting, which determines what the Tic will
/// do when there is a soft error.  Should be one of:
/// - TIC_RESPONSE_DEENERGIZE
/// - TIC_RESPONSE_HALT_AND_HOLD
/// - TIC_RESPONSE_DECEL_TO_STOP
/// - TIC_RESPONSE_GO_TO_POSITION
TIC_API
void tic_settings_set_soft_error_response(tic_settings *, uint8_t);

/// Gets the Soft error response setting described in
/// tic_settings_set_soft_error_response().
TIC_API
uint8_t tic_settings_get_soft_error_response(const tic_settings *);

/// Sets the Soft error position setting, which is the position the Tic will go
/// to when there is a soft error and its Soft error response parameter is set
/// to TIC_RESPONSE_GO_TO_POSITION.
TIC_API
void tic_settings_set_soft_error_position(tic_settings *, int32_t);

/// Gets the Input invalid position setting described in
/// tic_settings_set_soft_error_response().
TIC_API
int32_t tic_settings_get_soft_error_position(const tic_settings *);

/// Sets the baud rate in bits per second.
///
/// Only certain baud rates are actually achievable.  This function will change
/// the supplied baud rate to the nearest achievable baud rate.  You can call
/// tic_settings_get_serial_baud_rate() to see what baud rate was used.
TIC_API
void tic_settings_set_serial_baud_rate(tic_settings *, uint32_t);

/// Returns an estimate of the baud rate the controller will use for its
/// asynchronous serial port, in bits per second.
TIC_API
uint32_t tic_settings_get_serial_baud_rate(const tic_settings *);

/// Returns an acheivable baud rate corresponding to the specified baud rate.
/// Does not modify the settings object.
TIC_API
uint32_t tic_settings_achievable_serial_baud_rate(const tic_settings *, uint32_t);

/// Deprecated.  Use tic_settings_set_serial_device_number_u16 instead.
TIC_API __attribute__((deprecated))
void tic_settings_set_serial_device_number(tic_settings *, uint8_t);

/// Deprecated.  Use tic_settings_get_serial_device_number_u16 instead.
TIC_API __attribute__((deprecated))
uint8_t tic_settings_get_serial_device_number(const tic_settings *);

/// Sets the serial device number, a number that is used to identify the device
/// when using the Pololu serial protocol and is also used as the 7-bit I2C
/// address.
TIC_API
void tic_settings_set_serial_device_number_u16(tic_settings *, uint16_t);

/// Gets the serial device number setting described in
/// tic_settings_set_serial_device_number_u16().
TIC_API
uint16_t tic_settings_get_serial_device_number_u16(const tic_settings *);

/// Sets the alternative serial device number.
///
/// If the the alternative device number is disabled then this setting
/// will have no effect, but you can still store it on the Tic;
/// see tic_settings_set_serial_enable_alt_device_number().
TIC_API
void tic_settings_set_serial_alt_device_number(tic_settings *, uint16_t);

/// Gets the alternative serial device number setting described in
/// tic_settings_set_alt_serial_device_number().
TIC_API
uint16_t tic_settings_get_serial_alt_device_number(const tic_settings *);

/// Sets the "Enable alternative device number" setting.
///
/// When this is true, the Tic will obey commands sent to the alternative
/// device number (tic_settings_set_serial_alt_device_number()) as well as
/// the normal device number.
TIC_API
void tic_settings_set_serial_enable_alt_device_number(tic_settings *, bool);

/// Gets the "Enable alternative device number" settings described in
/// tic_settings_set_serial_enable_alt_device_number();
TIC_API
bool tic_settings_get_serial_enable_alt_device_number(const tic_settings *);

/// Sets the "Serial 14-bit device number" setting.  When true, the Tic expects
/// two device number bytes in each serial command.  The first contains bits
/// 0-6 of the device number and the second contains bits 7-13.
TIC_API
void tic_settings_set_serial_14bit_device_number(tic_settings *, bool);

/// Gets the "Serial 14-bit device number" setting described in
/// tic_settings_set_serial_14bit_device_number().
TIC_API
bool tic_settings_get_serial_14bit_device_number(const tic_settings *);

/// Sets the command timeout, the time in milliseconds before the device
/// considers it to be an error if we have not received certain commands.  A
/// value of 0 disables the command timeout feature.
TIC_API
void tic_settings_set_command_timeout(tic_settings *, uint16_t);

/// Gets the command timeout setting described in
/// tic_settings_set_command_timeout().
TIC_API
uint16_t tic_settings_get_command_timeout(const tic_settings *);

/// Deprecated.  Equivalent to tic_settings_set_serial_crc_for_commands().
TIC_API __attribute__((deprecated))
void tic_settings_set_serial_crc_enabled(tic_settings *, bool);

/// Deprecated.  Equivalent to tic_settings_get_serial_crc_for_commands().
TIC_API __attribute__((deprecated))
bool tic_settings_get_serial_crc_enabled(const tic_settings *);

/// Sets the "Serial CRC for commands" setting.  If true, the device requires
/// 7-bit CRC bytes on all serial commands.
TIC_API
void tic_settings_set_serial_crc_for_commands(tic_settings *, bool);

/// Gets the "Serial CRC for commands" setting described in
/// tic_settings_set_serial_crc_enabled().
TIC_API
bool tic_settings_get_serial_crc_for_commands(const tic_settings *);

/// Sets the "Serial CRC for responses" setting.  If true, the device adds
/// 7-bit CRC bytes for all serial responses less than 14 bytes in length.
TIC_API
void tic_settings_set_serial_crc_for_responses(tic_settings *, bool);

/// Gets the "Serial CRC for responses" setting described in
/// tic_settings_set_serial_crc_for_responses().
TIC_API
bool tic_settings_get_serial_crc_for_responses(const tic_settings *);

/// Sets the "Serial 7-bit responses" setting.  If true, the device will only
/// send bytes between 0 and 127 in response to serial commands, and serial
/// responses can contain at most 8 bytes (7 payload bytes, 1 byte with the
/// most-significant bits of the previous bytes).
TIC_API
void tic_settings_set_serial_7bit_responses(tic_settings *, bool);

/// Gets the serial 7-bit responses setting described in
/// tic_settings_get_serial_7bit_responses().
TIC_API
bool tic_settings_get_serial_7bit_responses(const tic_settings *);

/// Sets the serial response delay setting.  This specifies how long, in
/// microseconds, to delay before processing an I2C byte or sending a serial
/// response.  This delay is in addition to the time taken by normal processing.
TIC_API
void tic_settings_set_serial_response_delay(tic_settings *, uint8_t);

/// Gets the serial response delay setting described in
/// tic_settings_set_serial_response_delay().
TIC_API
uint8_t tic_settings_get_serial_response_delay(const tic_settings *);

/// Sets the VIN measurement calibration setting.
///
/// A higher number gives you higher VIN readings, while a lower number gives
/// you lower VIN readings.
TIC_API
void tic_settings_set_vin_calibration(tic_settings *, uint16_t);

/// Sets the VIN measurement calibratoin setting described in
/// tic_settings_set_vin_calibration().
TIC_API
uint16_t tic_settings_get_vin_calibration(const tic_settings *);


/// Sets the input averaging enabled setting.
///
/// If this setting is true and the control mode is set to an RC or analog
/// option, input averaging enabled, meaning that the controller will
/// keep a running average of the last four analog or RC input values and use
/// the average value to control the motor.
TIC_API
void tic_settings_set_input_averaging_enabled(tic_settings *, bool);

/// Gets the input averaging enabled setting as described in
/// tic_settings_set_input_averaging_enabled().
TIC_API
bool tic_settings_get_input_averaging_enabled(const tic_settings *);

/// Sets the input hysteresis setting.
///
/// This setting is described in the "Analog/RC input handling" section of the
/// Tic user's guide.
TIC_API
void tic_settings_set_input_hysteresis(tic_settings *, uint16_t);

/// Gets the input hysteresis setting as described in
/// tic_settings_set_input_hysteresis().
TIC_API
uint16_t tic_settings_get_input_hysteresis(const tic_settings *);

/// Sets the input scaling degree.
///
/// Should be TIC_SCALING_DEGREE_LINEAR, TIC_SCALING_DEGREE_QUADRATIC, or
/// TIC_SCALING_DEGREE_CUBIC.
TIC_API
void tic_settings_set_input_scaling_degree(tic_settings *, uint8_t);

/// Gets the input scaling degree setting described in
/// tic_settings_set_input_scaling_degree().
TIC_API
uint8_t tic_settings_get_input_scaling_degree(const tic_settings *);

/// Sets the input invert setting.  See tic_settings_set_output_max().
TIC_API
void tic_settings_set_input_invert(tic_settings *, bool);

/// Gets the input invert setting described in
/// tic_settings_set_input_invert().
TIC_API
bool tic_settings_get_input_invert(const tic_settings *);

/// Sets the input minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
void tic_settings_set_input_min(tic_settings *, uint16_t);

/// Gets the input minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
uint16_t tic_settings_get_input_min(const tic_settings *);

/// Sets the input neutral minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
void tic_settings_set_input_neutral_min(tic_settings *, uint16_t);

/// Gets the input neutral minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
uint16_t tic_settings_get_input_neutral_min(const tic_settings *);

/// Sets the input neutral maximum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
void tic_settings_set_input_neutral_max(tic_settings *, uint16_t);

/// Gets the input neutral maximum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
uint16_t tic_settings_get_input_neutral_max(const tic_settings *);

/// Sets the input maximum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
void tic_settings_set_input_max(tic_settings *, uint16_t);

/// Gets the input maximum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
uint16_t tic_settings_get_input_max(const tic_settings *);

/// Sets the output minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
void tic_settings_set_output_min(tic_settings *, int32_t);

/// Gets the output minimum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
int32_t tic_settings_get_output_min(const tic_settings *);

/// Sets the output maximum scaling parameter.
///
/// The following functions control the settings that define how an RC or analog
/// input gets mapped to a stepper motor position or speed:
///
/// - tic_settings_set_input_invert()
/// - tic_settings_set_input_min()
/// - tic_settings_set_input_neutral_min()
/// - tic_settings_set_input_neutral_max()
/// - tic_settings_set_input_max()
/// - tic_settings_set_output_min()
/// - tic_settings_set_output_max()
///
/// Input values between less than or equal to the input minimum are mapped to
/// the output minimum (or output maximum if the input is inverted).
///
/// Input values between the input minimum and input neutral minimum map to an
/// output value between the output minimum (or the output maximum if the input
/// is inverted) and 0.
///
/// Input values between the input neutral minimum and input netural maximum map
/// to 0.
///
/// Input values between the input neutral maximum and input maximum map to an
/// output value between 0 and output maximum (or output
/// minimum if the input is inverted).
///
/// Input values greater than or equal to the input maximum map to an output
/// value of output_max.
TIC_API
void tic_settings_set_output_max(tic_settings *, int32_t);

/// Gets the output maximum scaling parameter.  See
/// tic_settings_set_output_max().
TIC_API
int32_t tic_settings_get_output_max(const tic_settings *);

/// Sets the encoder prescaler.  For encoder control mode, this determines the
/// number of counts from the encoder per unit change of the stepper motor
/// position or speed.  The allowed values are 1 through
/// TIC_MAX_ALLOWED_ENCODER_PRESCALER.
TIC_API
void tic_settings_set_encoder_prescaler(tic_settings *, uint32_t);

/// Gets the encoder prescaler described in tic_settings_set_encoder_prescaler().
TIC_API
uint32_t tic_settings_get_encoder_prescaler(const tic_settings *);

/// Sets the encoder postscaler.  For encoder control mode, this determines the
/// size of a unit change in the stepper motor position or speed.  The allowed
/// values are 1 through TIC_MAX_ALLOWED_ENCODER_POSTSCALER.
TIC_API
void tic_settings_set_encoder_postscaler(tic_settings *, uint32_t);

/// Gets the encoder postscaler described in tic_settings_set_encoder_postscaler().
TIC_API
uint32_t tic_settings_get_encoder_postscaler(const tic_settings *);

/// Sets the encoder unlimited option.  If the Control mode is set to Encoder
/// position, normally the stepper motor position is limited by the output_min
/// and output_max settings.  However, if this setting is set to true, the
/// stepper motor's position will not be limited, and it should be able to turn
/// very far in either direction, even past the point where the encoder position
/// or the stepper motor position variables overflow.
TIC_API
void tic_settings_set_encoder_unlimited(tic_settings *, bool);

/// Gets the encoder unlimited option described in tic_settings_set_encoder_unlimited().
TIC_API
bool tic_settings_get_encoder_unlimited(const tic_settings *);

/// Sets the assigned function for a control pin.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
///
/// The func argument should be one of the TIC_PIN_FUNC_* macros.
TIC_API
void tic_settings_set_pin_func(tic_settings *, uint8_t pin, uint8_t func);

/// Gets the pin function as described in tic_settings_set_pin_func().
TIC_API
uint8_t tic_settings_get_pin_func(const tic_settings *, uint8_t pin);

/// Sets whether the pin will have its internal pull-up enabled (if applicable).
///
/// Note that this setting is ignored by the device if the pin function is
/// TIC_PIN_FUNC_DEFAULT.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
TIC_API
void tic_settings_set_pin_pullup(tic_settings *, uint8_t pin, bool pullup);

/// Gets the pullup flag for a pin as described in tic_settings_set_pin_pullup().
TIC_API
bool tic_settings_get_pin_pullup(const tic_settings *, uint8_t pin);

/// Sets whether the pin will be used as an analog input (if applicable).
///
/// Note that this setting is ignored by the device if the pin function is
/// TIC_PIN_FUNC_DEFAULT.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
TIC_API
void tic_settings_set_pin_analog(tic_settings *, uint8_t pin, bool analog);

/// Gets the analog flag for a pin as described in tic_settings_set_pin_analog().
TIC_API
bool tic_settings_get_pin_analog(const tic_settings *, uint8_t pin);

/// Sets whether the pin, when being used as a switch, is active high or active
/// low.
///
/// Note that this setting is ignored by the device if the pin function is
/// TIC_PIN_FUNC_DEFAULT.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
///
/// The polarity argument should be true if the pin is active high.
TIC_API
void tic_settings_set_pin_polarity(tic_settings *, uint8_t pin, bool polarity);

/// Gets the pin polarity as described in tic_settings_set_pin_polarity().
TIC_API
bool tic_settings_get_pin_polarity(const tic_settings *, uint8_t pin);

/// Sets the Invert motor direction setting.
///
/// If true, then forward/positive motion corresponds to the DIR pin being low.
/// Normally, forward/positive motion corresponds to the DIR pin being high.
TIC_API
void tic_settings_set_invert_motor_direction(tic_settings *, bool);

/// Gets the Invert motor direction setting describe in
/// tic_settings_set_invert_motor_direction().
TIC_API
bool tic_settings_get_invert_motor_direction(const tic_settings *);

/// Sets the maximum speed, or speed limit, in steps per 10000 seconds.  Valid
/// values are from 0 to 500000000 (50 kHz).  Values between 0 and 6 behave the
/// same as 0.
///
/// This sets the default value.  To set or get the current value, see:
///
/// - tic_set_max_speed()
/// - tic_variables_get_max_speed()
TIC_API
void tic_settings_set_max_speed(tic_settings *, uint32_t);

/// Gets the speed maximum described in tic_settings_set_max_speed().
TIC_API
uint32_t tic_settings_get_max_speed(const tic_settings *);

/// Sets the starting speed in microsteps per 10000 seconds.
///
/// Valid values are from -500000000 to 500000000.
///
/// This sets the default value.  To set or get the current value, see:
///
/// - tic_set_starting_speed()
/// - tic_variables_get_starting_speed()
TIC_API
void tic_settings_set_starting_speed(tic_settings *, uint32_t);

/// Gets the starting speed described in tic_settings_set_starting_speed().
TIC_API
uint32_t tic_settings_get_starting_speed(const tic_settings *);

/// Sets the maximum acceleration, or acceleration limit, in steps per 100
/// square seconds.  Valid values are 59 to 2147483647.
///
/// This sets the default value.  To set or get the current value, see:
///
/// - tic_set_max_accel()
/// - tic_variables_get_max_accel()
TIC_API
void tic_settings_set_max_accel(tic_settings *, uint32_t);

/// Gets the acceleration maximum described in tic_settings_set_max_accel().
TIC_API
uint32_t tic_settings_get_max_accel(const tic_settings *);

/// Sets the maximum deceleration, or deceleration limit, in steps per 100
/// square seconds.  Valid values are 0 and 59 to 2147483647.  A value of 0
/// causes the controller's deceleration limit to be equal to the acceleration
/// limit.
///
/// This sets the default value.  To set or get the current value, see:
///
/// - tic_set_max_decel()
/// - tic_variables_get_max_decel()
TIC_API
void tic_settings_set_max_decel(tic_settings *, uint32_t);

/// Gets the maximum deceleration described in tic_settings_set_max_decel().
TIC_API
uint32_t tic_settings_get_max_decel(const tic_settings *);

/// Sets the default step mode, also known as the microstepping mode.  This
/// should be one of the TIC_STEP_MODE_* macros, but not all step modes are
/// supported on all products.  If you specify an invalid or unsupported mode to
/// this function, you can fix it with tic_settings_fix().
TIC_API
void tic_settings_set_step_mode(tic_settings *, uint8_t);

/// Gets the step mode described in
/// tic_settings_set_step_mode().
TIC_API
uint8_t tic_settings_get_step_mode(const tic_settings *);

/// Sets the default stepper motor coil current limit in milliamps.
TIC_API
void tic_settings_set_current_limit(tic_settings *, uint32_t);

/// Gets the current limit setting as described in
/// tic_settings_set_current_limit().
TIC_API
uint32_t tic_settings_get_current_limit(const tic_settings *);

/// Sets the default stepper motor coil current code.
///
/// To calculate the code from milliamps, see tic_current_limit_ma_to_code().
/// To convert the code to milliamps, see tic_current_limit_code_to_ma().
///
/// See also tic_settings_set_current_limit().
TIC_API
void tic_settings_set_current_limit_code(tic_settings *, uint8_t);

/// Gets the default stepper motor coil current code, as described in
/// tic_settings_set_current_limit_code().
TIC_API
uint8_t tic_settings_get_current_limit_code(const tic_settings *);

/// Sets the stepper motor coil current limit in milliamps when there is an
/// error.
///
/// A value of -1 means to use the default current limit.
TIC_API
void tic_settings_set_current_limit_during_error(tic_settings *, int32_t);

/// Gets the current limit during error setting as described in
/// tic_settings_set_current_limit_during_error().
TIC_API
int32_t tic_settings_get_current_limit_during_error(const tic_settings *);

/// Sets the stepper motor coil current code when there is an error.
///
/// A value of 0xFF means to use the default current limit.
///
/// Otherwise, to calculate the code from milliamps, see
/// tic_current_limit_ma_to_code().
///
/// To convert the code to milliamps, see tic_current_limit_code_to_ma().
///
/// See also tic_settings_set_current_limit_during_error().
TIC_API
void tic_settings_set_current_limit_code_during_error(tic_settings *, uint8_t);

/// Gets the stepper motor coil current code when there is an error, as
/// described in tic_settings_set_current_limit_code_during_error().
TIC_API
uint8_t tic_settings_get_current_limit_code_during_error(const tic_settings *);

/// Returns the highest achievable current limit that is less than the given
/// current limit.  Does not modify the settings object.
TIC_API
uint32_t tic_settings_achievable_current_limit(const tic_settings *, uint32_t);

/// Sets the decay mode.  The argument should be TIC_DECAY_MIXED,
/// TIC_DELAY_SLOW, or TIC_DECAY_FAST.
TIC_API
void tic_settings_set_decay_mode(tic_settings *, uint8_t);

/// Gets the decay mode described in tic_settings_get_decay_mode().
TIC_API
uint8_t tic_settings_get_decay_mode(const tic_settings *);

/// Sets the "Enable automatic homing" setting.
///
/// If true, the Tic will do the homing procedure whenever it is in normal
/// operation mode, trying to get to a target position, and the position
/// uncertain flag is set.  This setting was added in firmware version 1.06.
TIC_API
void tic_settings_set_auto_homing(tic_settings *, bool);

/// Gets the setting described in tic_settings_set_auto_homing().
TIC_API
bool tic_settings_get_auto_homing(const tic_settings *);

/// Sets the auto homing forward setting, also known as
/// "Automatic homing direction" in the GUI.
///
/// If true, the primary direction that the Tic moves during the auto-homing
/// procedure will be forward.  Otherwise, it will be reverse.
TIC_API
void tic_settings_set_auto_homing_forward(tic_settings *, bool);

/// Gets the auto homing forward setting described in
/// tic_settings_set_auto_homing_forward().
TIC_API
bool tic_settings_get_auto_homing_forward(const tic_settings *);

/// Sets the speed to use while moving towards a limit switch during homing,
/// in microsteps per 10000 seconds.
TIC_API
void tic_settings_set_homing_speed_towards(tic_settings *, uint32_t);

/// Gets the setting described in tic_settings_get_homing_speed_towards().
TIC_API
uint32_t tic_settings_get_homing_speed_towards(const tic_settings *);

/// Sets the speed to use while moving away from a limit switch during homing,
/// in microsteps per 10000 seconds.
TIC_API
void tic_settings_set_homing_speed_away(tic_settings *, uint32_t);

/// Gets the setting described in tic_settings_get_homing_speed_away().
TIC_API
uint32_t tic_settings_get_homing_speed_away(const tic_settings *);

/// Sets the AGC mode.  The argument should be TIC_AGC_MODE_OFF,
/// TIC_AGC_MODE_ON, or TIC_AGC_MODE_ACTIVE_OFF.
///
/// This setting is only valid for the Tic T249.
TIC_API
void tic_settings_set_agc_mode(tic_settings *, uint8_t mode);

/// Gets the AGC mode described in tic_settings_set_agc_mode().
TIC_API
uint8_t tic_settings_get_agc_mode(const tic_settings *);

/// Sets the AGC bottom current limit.
///
/// The limit argument should be one of:
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_45
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_50
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_55
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_60
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_65
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_70
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_75
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_80
///
/// This setting is only valid for the Tic T249.
TIC_API
void tic_settings_set_agc_bottom_current_limit(tic_settings *, uint8_t limit);

/// Gets the AGC bottom current limit described in
/// tic_settings_set_agc_bottom_current_limit().
TIC_API
uint8_t tic_settings_get_agc_bottom_current_limit(const tic_settings *);

/// Sets the AGC current boost steps.
///
/// The steps argument should be one of:
/// - TIC_AGC_CURRENT_BOOST_STEPS_5
/// - TIC_AGC_CURRENT_BOOST_STEPS_7
/// - TIC_AGC_CURRENT_BOOST_STEPS_9
/// - TIC_AGC_CURRENT_BOOST_STEPS_11
///
/// This setting is only valid for the Tic T249.
TIC_API
void tic_settings_set_agc_current_boost_steps(tic_settings *, uint8_t steps);

/// Gets the AGC current boost steps described in
/// tic_settings_set_agc_current_boost_steps().
TIC_API
uint8_t tic_settings_get_agc_current_boost_steps(const tic_settings *);

/// Sets the AGC frequency limit.
///
/// The limit argument should be one of:
/// - TIC_AGC_FREQUENCY_LIMIT_OFF
/// - TIC_AGC_FREQUENCY_LIMIT_225
/// - TIC_AGC_FREQUENCY_LIMIT_450
/// - TIC_AGC_FREQUENCY_LIMIT_675
///
/// This setting is only valid for the Tic T249.
TIC_API
void tic_settings_set_agc_frequency_limit(tic_settings *, uint8_t limit);

/// Gets the AGC frequency limit described in
/// tic_settings_set_agc_frequency_limit().
TIC_API
uint8_t tic_settings_get_agc_frequency_limit(const tic_settings *);

/// Sets the "Enable unrestricted current limits" setting (Tic 36v4 only).
///
/// By default, the Tic 36v4 caps its current limit at 3939 mA (which is
/// encoded as 55).  Any attempt to set a current limit above that, whether
/// from a setting or a command, results in 3939 mA.
///
/// If this setting is enabled, that limit is disabled, and currents can go
/// all the way up to 9095 mA (encoded as 127).
///
/// WARNING: The Tic 36v4 has no meaningful over-temperature shut-off
/// (while the gate driver IC has over-temperature protection, it is the
/// external MOSFETs that will overheat first).  An over-temperature condition
/// can cause permanent damage to the motor driver.  We strongly recommend you
/// do not increase the current limit setting beyond 4000&nbsp;mA (or lower in
/// applications with reduced heat dissipation) unless you can first confirm
/// that the temperature of the MOSFETs will stay under 140°C.
TIC_API
void tic_settings_set_hp_enable_unrestricted_current_limits(
  tic_settings *, bool enable);

/// Gets the "Enable unrestricted current limits" setting for the Tic 36v4,
/// as described in tic_settings_set_hp_enable_unrestricted_current_limits().
TIC_API
bool tic_settings_get_hp_enable_unrestricted_current_limits(
  const tic_settings *);

/// Sets the fixed off time (Tic 36v4 only).
///
/// This sets the fixed off time in increments of 500 nanoseconds,
/// with 0 corresponding to 500 ns.
/// Any value between 0 (500 ns) and 255 (1280 us) is allowed.
///
/// This setting is only valid for the Tic 36v4.
TIC_API
void tic_settings_set_hp_toff(tic_settings *, uint8_t time);

/// Gets the fixed off time setting for the Tic 36v4, as described in
/// tic_settings_set_hp_toff().
TIC_API
uint8_t tic_settings_get_hp_toff(const tic_settings *);

/// Sets the current trip blanking time (Tic 36v4 only).
///
/// This sets the current trip blanking time, in increments of 0.02 us.
///
/// Any value between 0 and 255 is valid.
/// Any value between 0 and 50 corresponds to 1 us.
/// Any value above 50 corresponds to 0.02 us times the value, so a value of
/// 255 corresponds to 5.1 us.
///
/// This setting is only valid for the Tic 36v4.
TIC_API
void tic_settings_set_hp_tblank(tic_settings *, uint8_t time);

/// Gets the current trip blanking time for the Tic 36v4, as described in
/// tic_settings_set_hp_tblank().
TIC_API
uint8_t tic_settings_get_hp_tblank(const tic_settings *);

/// Enables or disables adaptive blanking time for a Tic 36v4.
///
/// This setting is only valid for the Tic 36v4.
TIC_API
void tic_settings_set_hp_abt(tic_settings *, bool adaptive);

/// Gets the value of the Tic 36v4 adaptive blanking time setting, as described
/// in tic_settings_get_hp_abt().
TIC_API
bool tic_settings_get_hp_abt(const tic_settings *);

/// Sets the mixed decay transition time of the Tic 36v4.
///
/// This sets the mixed decay transition time, in units of 0.5 us.
///
/// Any value between 0 (0.5 us) and 255 (127.5 us) is valid.
///
/// This setting is only valid for the Tic 36v4.
TIC_API
void tic_settings_set_hp_tdecay(tic_settings *, uint8_t time);

/// Gets the mixed decay transition time of the Tic 36v4, as described in
/// tic_settings_get_hp_tdecay().
TIC_API
uint8_t tic_settings_get_hp_tdecay(const tic_settings *);

/// Sets the decay mode of the Tic 36v4.
///
/// The /p mode argument should be one of the TIC_HP_DECMOD_* macros.
///
/// This setting is only valid for the Tic 36v4.
/// If you are using a different Tic, see tic_settings_set_decay_mode().
TIC_API
void tic_settings_set_hp_decmod(tic_settings *, uint8_t mode);

/// Gets the decay mode of the Tic 36v4, as described in
/// tic_settings_set_hp_decmod().
TIC_API
uint8_t tic_settings_get_hp_decmod(const tic_settings *);


// tic_variables ///////////////////////////////////////////////////////////////

/// Represents run-time variables that have been read from the Tic using the Get
/// Variables command.
typedef struct tic_variables tic_variables;

/// Copies a tic_variables object.  If this function is successful, the caller must
/// free the settings later by calling tic_settings_free().
TIC_API TIC_WARN_UNUSED
tic_error * tic_variables_copy(
  const tic_variables * source,
  tic_variables ** dest);

/// Frees a tic_variables object.  It is OK to pass a NULL pointer to this
/// function.  Do not free the same non-NULL settings object twice.
TIC_API
void tic_variables_free(tic_variables *);

/// Returns the current operation state of the Tic, which will be one of the
/// TIC_OPERATION_STATE_* macros.
TIC_API
uint8_t tic_variables_get_operation_state(const tic_variables *);

/// Returns true if the motor driver is energized (trying to send current to its
/// outputs).
TIC_API
bool tic_variables_get_energized(const tic_variables *);

/// Gets a flag that indicates whether there has been external confirmation that
/// the value of the Tic's "Current position" variable is correct.
///
/// For more information, see the "Error handling" section of the Tic user's
/// guide.
TIC_API
bool tic_variables_get_position_uncertain(const tic_variables *);

/// Returns true if a forward limit switch is active.  This is just a function
/// of the digital inputs, not whether the limit switch is currently having an
/// effect on the motor.
TIC_API
bool tic_variables_get_forward_limit_active(const tic_variables *);

/// Returns true if a reverse limit switch is active.  This is just a function
/// of the digital inputs, not whether the limit switch is currently having an
/// effect on the motor.
TIC_API
bool tic_variables_get_reverse_limit_active(const tic_variables *);

/// Returns true if the Tic's homing system is currently active.
///
/// See tic_go_home().
TIC_API
bool tic_variables_get_homing_active(const tic_variables * variables);

/// Gets the errors that are currently stopping the motor.
///
/// This is a bitmask, and the bits are defined by the TIC_ERROR_* macros.
TIC_API
uint16_t tic_variables_get_error_status(const tic_variables *);

/// Gets the errors that have occurred since the last time these error occurred
/// bits were cleared.
///
/// This is a bitmask, and the bits are defined by the TIC_ERROR_* macros.  To
/// learn more about what the different errors mean, see the Tic user's guide.
TIC_API
uint32_t tic_variables_get_errors_occurred(const tic_variables *);

/// Gets the current planning mode.
///
/// This will be either TIC_PLANNING_MODE_OFF,
/// TIC_PLANNING_MODE_TARGET_POSITION, or TIC_PLANNING_MODE_TARGET_VELOCITY.
TIC_API
uint8_t tic_variables_get_planning_mode(const tic_variables *);

/// Gets the target position, in microsteps.
///
/// This is only relevant if the planning mode from
/// tic_variables_get_planning_mode() is TIC_PLANNING_MODE_TARGET_POSITION.
TIC_API
int32_t tic_variables_get_target_position(const tic_variables *);

/// Gets the target velocity, in microsteps per 10000 seconds.
///
/// This is only relevant if the planning mode from
/// tic_variables_get_planning_mode() is TIC_PLANNING_MODE_TARGET_VELOCITY.
TIC_API
int32_t tic_variables_get_target_velocity(const tic_variables *);

/// Gets the current maximum speed, in microsteps per 10000 seconds.
///
/// This is the current value, which could differ from the value in the Tic's
/// settings.
///
/// See also tic_settings_set_max_speed() and tic_set_max_speed().
TIC_API
uint32_t tic_variables_get_max_speed(const tic_variables *);

/// Gets the starting speed in microsteps per 10000 seconds.
///
/// This is the current value, which could differ from the value in the
/// Tic's settings.
///
/// See also tic_settings_set_starting_speed() and tic_set_starting_speed().
TIC_API
uint32_t tic_variables_get_starting_speed(const tic_variables *);

/// Gets the current maximum acceleration, in microsteps per second per 100
/// seconds.
///
/// This is the current value, which could differ from the value in the Tic's
/// settings.
///
/// See also tic_settings_set_max_accel() and tic_set_max_accel().
TIC_API
uint32_t tic_variables_get_max_accel(const tic_variables *);

/// Gets the current maximum deceleration, in microsteps per second per 100
/// seconds.
///
/// This is the current value, which could differ from the value in the Tic's
/// settings.
///
/// See also tic_settings_set_max_decel() and tic_set_max_decel().
TIC_API
uint32_t tic_variables_get_max_decel(const tic_variables *);

/// Gets the current position of the stepper motor, in microsteps.
///
/// Note that this just tracks steps that the Tic has commanded the stepper
/// driver to take; it could be different from the actual position of the motor
/// for various reasons.
///
/// See also tic_halt_and_set_position().
TIC_API
int32_t tic_variables_get_current_position(const tic_variables *);

/// Gets the current velocity of the stepper motor, in microsteps per 10000
/// seconds.
///
/// Note that this is just the velocity used in the Tic's step planning
/// algorithms, and it might not correspond to the actual velocity of the motor
/// for various reasons.
TIC_API
int32_t tic_variables_get_current_velocity(const tic_variables *);

/// Gets the acting target position, in microsteps.
///
/// This is a variable used in the Tic's target position step planning
/// algorithm, and it could be invalid while the motor is stopped.
TIC_API
int32_t tic_variables_get_acting_target_position(const tic_variables *);

/// Gets the time since the last step, in timer ticks.
///
/// Each timer tick represents one third of a microsecond.  The Tic only updates
/// this variable every 5 milliseconds or so.
TIC_API
uint32_t tic_variables_get_time_since_last_step(const tic_variables *);

/// Gets the cause of the controller's last reset.
///
/// This will be one the TIC_RESET_* macros.
TIC_API
uint8_t tic_variables_get_device_reset(const tic_variables *);

/// Gets the current measurement of the VIN voltage, in millivolts.
TIC_API
uint32_t tic_variables_get_vin_voltage(const tic_variables *);

/// Gets the time since the last full reset of the Tic's microcontroller, in
/// milliseconds.
///
/// A Reset command does not count.
TIC_API
uint32_t tic_variables_get_up_time(const tic_variables *);

/// Gets the reading from the quadrature encoder input, in ticks.
TIC_API
int32_t tic_variables_get_encoder_position(const tic_variables *);

/// Gets the reading from the RC pulse input, in units of twelfth microseconds.
///
/// The value returned will be TIC_INPUT_NULL if the RC signal is bad or has not
/// been measured.
TIC_API
uint16_t tic_variables_get_rc_pulse_width(const tic_variables *);

/// Gets the current step mode of the Tic.
///
/// Note that this is the current mode.  To get the default mode at startup,
/// see tic_settings_get_step_mode().
///
/// The return value is one of the TIC_STEP_MODE_* macros.
TIC_API
uint8_t tic_variables_get_step_mode(const tic_variables *);

/// Gets the stepper motor coil current limit in milliamps.
///
/// This is the value being used now, which could differ from the value in the
/// Tic's settings.
///
/// See also tic_settings_set_current_limit(), tic_set_current_limit().
TIC_API
uint32_t tic_variables_get_current_limit(const tic_variables *);

/// Gets the current limit code.
///
/// This is the value being used now, whcih could differ from the value in the
/// Tic' settings.
///
/// To convert this to milliamps, use tic_current_limit_code_to_ma().
///
/// See also tic_settings_set_current_limit_code(),
/// tic_settings_get_current_limit_code(), tic_set_current_limit().
TIC_API
uint8_t tic_variables_get_current_limit_code(const tic_variables *);

/// Gets the current decay mode of the Tic.
///
/// Note that this is the current decay mode.  To get the default decay mode at
/// startup, see tic_settings_get_decay_mode().
///
/// The return value is one of the TIC_DECAY_MODE_* macros.
TIC_API
uint8_t tic_variables_get_decay_mode(const tic_variables *);

/// Gets the current state of the control input to Tic.
///
/// This will be one of the TIC_INPUT_STATE_* macros.
TIC_API
uint8_t tic_variables_get_input_state(const tic_variables *);

/// Gets a variable used in the process that converts raw RC and analog values
/// into a motor position or speed.  This is mainly for debugging input scaling
/// settings in an RC or analog mode.
///
/// A value of TIC_INPUT_NULL means the input value is not available.
TIC_API
uint16_t tic_variables_get_input_after_averaging(const tic_variables *);

/// Gets a variable used in the process that converts raw RC and analog values
/// into a motor position or speed.  This is mainly for debugging input scaling
/// settings in an RC or analog mode.
///
/// A value of TIC_INPUT_NULL means the input value is not available.
TIC_API
uint16_t tic_variables_get_input_after_hysteresis(const tic_variables *);

/// Gets the number that is fed into the input scaling routine.  This is just
/// the input_after_hysteresis variable divided by a power of two.  The power of
/// two depends on the control mode and whether input averaging is enabled, so
/// this function needs the Tic's settings as an argument.
TIC_API
uint16_t tic_variables_get_input_before_scaling(const tic_variables *,
  const tic_settings *);

/// Gets a variable used in the process that converts raw RC and analog values
/// into a motor position or speed.
///
/// A value of TIC_INPUT_NULL means the input value is not available.
TIC_API
int32_t tic_variables_get_input_after_scaling(const tic_variables *);

/// Gets the type of the last motor driver error that occurred or 0 if no
/// motor driver errors have occurred since starting up.
///
/// This is only valid for the Tic T249, and will be TIC_MOTOR_DRIVER_ERROR_NONE
/// for all other Tics.
///
/// The return value is one of the TIC_MOTOR_DRIVER_ERROR_* macros.
TIC_API
uint8_t tic_variables_get_last_motor_driver_error(const tic_variables *);

/// Gets the current AGC mode of the Tic.
///
/// Note that this is the current AGC mode.  To get the default AGC mode at
/// startup, see tic_settings_get_agc_mode().
///
/// This is only valid for the Tic T249, and will be 0 for all other Tics.
///
/// The return value is one of the TIC_AGC_MODE_* macros.
TIC_API
uint8_t tic_variables_get_agc_mode(const tic_variables *);

/// Gets the current AGC bottom current limit of the Tic.
///
/// Note that this is the AGC bottom current limit that the Tic is using
/// presently.  To get the default at startup, see
/// tic_settings_get_agc_bottom_current_limit().
///
/// This is only valid for the Tic T249, and will be 0 for all other Tics.
///
/// The return value is one of the TIC_AGC_BOTTOM_CURRENT_LIMIT_* macros.
TIC_API
uint8_t tic_variables_get_agc_bottom_current_limit(const tic_variables *);

/// Gets the current AGC current boost steps of the Tic.
///
/// Note that this is the AGC current boost steps value that the Tic is using
/// presently.  To get the default at startup, see
/// tic_settings_get_agc_current_boost_steps().
///
/// This is only valid for the Tic T249, and will be 0 for all other Tics.
///
/// The return value is one of the TIC_AGC_CURRENT_BOOST_STEPS_* macros.
TIC_API
uint8_t tic_variables_get_agc_current_boost_steps(const tic_variables *);

/// Gets the current AGC frequency limit of the Tic.
///
/// Note that this is the AGC frequency limit that the Tic is using presently.
/// To get the default at startup, see tic_settings_get_agc_frequency_limit().
///
/// This is only valid for the Tic T249, and will be 0 for all other Tics.
///
/// The return value is one of the TIC_AGC_FREQUENCY_LIMIT_* macros.
TIC_API
uint8_t tic_variables_get_agc_frequency_limit(const tic_variables *);

/// Gets the analog reading from the specified pin, if analog readings are
/// enabled for that pin.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
///
/// The return value will be a left-justified analog reading; a value of 0
/// represents 0 V and a value near 0xFFFE represents the voltage on the
/// controller's 5V pin.  A value of TIC_INPUT_NULL means the reading is not
/// available.
TIC_API
uint16_t tic_variables_get_analog_reading(const tic_variables *, uint8_t pin);

/// Gets the digital reading for the specified pin.
///
/// The pin argument should be one of the TIC_PIN_NUM_* macros.
TIC_API
bool tic_variables_get_digital_reading(const tic_variables *, uint8_t pin);

/// Gets the pin state for the specified pin, i.e. what kind of input or output
/// it is.
///
/// Note that the state might be misleading if the pin is being used as a serial
/// or I2C pin.
///
/// This pin argument should be one of the TIC_PIN_NUM_* macros.
///
/// The return value is one of the TIC_PIN_STATE_* macros.
TIC_API
uint8_t tic_variables_get_pin_state(const tic_variables *, uint8_t pin);

/// Gets a bitmask describing the error conditions that caused the last
/// motor driver error.
///
/// This is only valid for the Tic 36v4, and will be 0 for all
/// other Tics.
///
/// See tic_look_up_hp_driver_error_name_ui().
TIC_API
uint32_t tic_variables_get_last_hp_driver_errors(const tic_variables *);

// Undocumented function for testing.  Not part of the public API.
TIC_API
tic_variables * tic_variables_fake(void);


// tic_device ///////////////////////////////////////////////////////////////////

/// Represents a Tic that is or was connected to the computer.
///
typedef struct tic_device tic_device;

/// Finds all the Tic devices connected to the computer via USB and returns a
/// list of them.
///
/// The list is terminated by a NULL pointer.  The optional @a device_count
/// parameter is used to return the number of devices in the list.
///
/// If this function is successful (the returned error pointer is NULL), then
/// you must later free each device by calling tic_device_free() and free the
/// list by calling tic_list_free().  The order in which the list and the
/// retrieved objects are freed does not matter.
TIC_API TIC_WARN_UNUSED
tic_error * tic_list_connected_devices(
  tic_device *** device_list,
  size_t * device_count);

/// Frees a device list returned by ::tic_list_connected_devices.  It is OK to
/// pass NULL to this function.
TIC_API
void tic_list_free(tic_device ** list);

/// Makes a copy of a device object.  If this function is successful, you will
/// need to free the copy by calling tic_device_free() at some point.
TIC_API TIC_WARN_UNUSED
tic_error * tic_device_copy(
  const tic_device * source,
  tic_device ** dest);

/// Frees a device object.  Passing a NULL pointer to this function is OK.  Do
/// not free the same non-NULL pointer twice.
TIC_API
void tic_device_free(tic_device *);

/// Gets the product code of the device, which will be one of the TIC_PRODUCT_*
/// macros.  If the device is NULL, returns 0 instead.
TIC_API TIC_WARN_UNUSED
uint8_t tic_device_get_product(const tic_device *);

/// Gets the full name of the device (e.g. "Tic USB Stepper Motor Controller
/// T825") as an ASCII-encoded string.  The string will be valid at least as
/// long as the device object.
/// If the device is NULL, returns an empty string.
TIC_API TIC_WARN_UNUSED
const char * tic_device_get_name(const tic_device *);

/// Gets the short name of the device (e.g. "T825") as an ASCII-encoded string.
/// The string will be valid at least as long as the device object.
/// If the device is NULL, returns an empty string.
TIC_API TIC_WARN_UNUSED
const char * tic_device_get_short_name(const tic_device *);

/// Gets the serial number of the device as an ASCII-encoded string.
/// The string will be valid at least as long as the device object.
/// If the device is NULL, returns an empty string.
TIC_API TIC_WARN_UNUSED
const char * tic_device_get_serial_number(const tic_device *);

/// Get an operating system-specific identifier for this device as an
/// ASCII-encoded string.  The string will be valid at least as long as the
/// device object.
/// If the device is NULL, returns an empty string.
TIC_API TIC_WARN_UNUSED
const char * tic_device_get_os_id(const tic_device *);

/// Gets the firmware version as a binary-coded decimal number (e.g. 0x0100
/// means "1.00").  We recommend using tic_get_firmware_version_string() instead
/// of this function if possible.
TIC_API TIC_WARN_UNUSED
uint16_t tic_device_get_firmware_version(const tic_device *);


// tic_handle ///////////////////////////////////////////////////////////////////

/// Represents an open handle that can be used to read and write data from a
/// device.
typedef struct tic_handle tic_handle;

/// Opens a handle to the specified device.  The handle must later be closed
/// with tic_handle_close().
///
/// On Windows, if another applications has a handle open already, then this
/// function will fail and the returned error will have code
/// ::TIC_ERROR_ACCESS_DENIED.
TIC_API TIC_WARN_UNUSED
tic_error * tic_handle_open(const tic_device *, tic_handle **);

/// Closes and frees the specified handle.  It is OK to pass NULL to this
/// function.  Do not close the same non-NULL handle twice.
TIC_API
void tic_handle_close(tic_handle *);

/// Gets the device object that corresponds to this handle.
/// The device object will be valid for at least as long as the handle.
TIC_API TIC_WARN_UNUSED
const tic_device * tic_handle_get_device(const tic_handle *);

/// Gets the firmware version string, including any special modification codes
/// (e.g. "1.07nc").  The string will be valid for at least as long as the device.
///
/// This function might need to do I/O, but it will cache the result so it is OK
/// to call it frequently.
TIC_API TIC_WARN_UNUSED
const char * tic_get_firmware_version_string(tic_handle *);

/// Sets the target position of the Tic, in microsteps.
///
/// This function sends a set target position to the Tic.  If the Control mode
/// is set to Serial/I2C/USB, the Tic will start moving the motor to reach the
/// target position.
///
/// If the control mode is something other than Serial, this command will be
/// silently ignored.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_target_position(tic_handle *, int32_t position);

/// Sets the target velocity of the Tic, in microsteps per 10000 seconds.
///
/// This function sends a set target velocity command to the Tic.  If the
/// Control mode is set to Serial/I2C/USB, the Tic will start accelerating or
/// decelerating to reach the target velocity.
///
/// If the control mode is something other than Serial, this command will be
/// silently ignored.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_target_velocity(tic_handle *, int32_t velocity);

/// Stops the motor abruptly without respecting the deceleration limit ands sets
/// the "Current position" variable, which represents where the Tic currently
/// thinks the motor's output is.
///
/// This function sends a halt and set position command to the Tic.  Besides
/// stopping the motor and setting the current position, this command also
/// clears the "Learn position later" flag, sets the "Input state" to "halt",
/// and clears the "Input after scaling" variable.
///
/// If the control mode is something other than Serial/I2C/USB, ths
/// command will be silently ignored.
TIC_API TIC_WARN_UNUSED
tic_error * tic_halt_and_set_position(tic_handle *, int32_t position);

/// Stops the motor abruptly without respecting the deceleration limit.
///
/// This function sends a halt and hold command to the Tic.  Besides stopping
/// the motor, this command also sets the "Input state" to "halt", and clears
/// the "Input after scaling" variable.
///
/// If the control mode is something other than Serial/I2C/USB, ths command will
/// be silently ignored.
///
/// See also tic_deenergize().
TIC_API TIC_WARN_UNUSED
tic_error * tic_halt_and_hold(tic_handle *);

/// Sends a "Go Home" command, starting the Tic's homing procedure.
///
/// The direction argument should be 1 for forward or 0 for reverse.
TIC_API TIC_WARN_UNUSED
tic_error * tic_go_home(tic_handle *, uint8_t direction);

/// Prevents the "Command timeout" error from happening for some time.
///
/// This function sends a reset command timeout command to the Tic.
TIC_API TIC_WARN_UNUSED
tic_error * tic_reset_command_timeout(tic_handle *);

/// Deenergizes the stepper motor coils.
///
/// This function sends a De-energize command to the Tic, causing it to disable
/// its stepper motor driver.  The motor will stop moving and consuming power.
/// The Tic will also set the "Intentionally de-energized" error bit, turn on
/// its red LED, and drive its ERR line high.
///
/// Note that the Energize command, which can be sent over USB with
/// tic_energize() or sent over serial or I2C, will undo the effect of this
/// command and could make the system start up again.
///
/// See also tic_halt_and_hold().
TIC_API TIC_WARN_UNUSED
tic_error * tic_deenergize(tic_handle *);

/// Sends the Energize command.
///
/// This function sends an Energize command to the Tic, clearing the
/// "Intentionally deenergized" error bit.
///
/// Note that this function is not normally needed to use the Tic: it is only
/// needed if the Tic previously received a "De-energize" command.
TIC_API TIC_WARN_UNUSED
tic_error * tic_energize(tic_handle *);

/// Sends the "Exit safe start" command.
///
/// In Serial/I2C/USB control mode, this command causes the Safe Start Violation
/// error to be cleared for 200 ms.  If there are no other errors, this
/// allows the system to start up.
TIC_API TIC_WARN_UNUSED
tic_error * tic_exit_safe_start(tic_handle *);

/// Sends the "Enter safe start" command.
///
/// This command has no effect if safe-start is disabled in the Tic settings (see
/// tic_settings_set_disable_safe_start()).
///
/// In Serial/I2C/USB control mode, this command causes the Tic to stop the
/// motor and set its safe start violation error bit.  An Exit Safe Start
/// command is required before the Tic will move the motor again.
///
/// In RC speed, Analog speed, or Encoder speed control modes, this command
/// causes the Tic to stop the motor and set its Safe Start Violation error bit.
/// The control input needs to move to its neutral position before the Tic will
/// move the motor again.
///
/// In the RC Position, Analog Position, or Encoder Position control modes, this
/// command is not useful.  It will set the safe start violation error bit,
/// causing the motor to stop, but the error bit will be cleared very quickly
/// and the motor will continue moving.
TIC_API TIC_WARN_UNUSED
tic_error * tic_enter_safe_start(tic_handle *);

/// Sends the Reset command.
///
/// This command makes the Tic forget most parts of its current state.
///
/// This is similar to the tic_reinitialize() command that is used to apply new
/// settings.  Both commands cause the Tic to reload its settings, so it will go
/// back to using its default step mode, current limit, decay mode, speed max,
/// starting speed, accel max, and decel max settings.
///
/// Unlike tic_reinitialize(), which applies new settings seamlessly if
/// possible, the tic_reset() command always abruptly stops the motor, resets
/// the motor driver, sets the Tic's Operation state to "Reset", clears the last
/// movement command, clears the encoder position, clears the Serial Error bit,
/// clears the Command Timeout error bit, and clears the Errors Occurred bits.
///
/// Neither command modifies the Intentionally de-energized error bit.
///
/// The Tic's serial and I2C interfaces will be unreliable for a brief period
/// after the Tic receives the Reset command, so we recommend waiting at least
/// 10 ms before sending I2C or serial commands after sending a Reset command.
TIC_API TIC_WARN_UNUSED
tic_error * tic_reset(tic_handle *);

/// Attempts to clear a motor driver error.
///
/// This function sends a "Clear driver error" command to the Tic.
TIC_API TIC_WARN_UNUSED
tic_error * tic_clear_driver_error(tic_handle *);

/// Temporarily sets the maximum speed, in units of steps per 10000 seconds.
///
/// This function sends a "Set max speed command" to the Tic.  For more
/// information, see tic_settings_set_max_speed().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_max_speed(tic_handle *, uint32_t max_speed);

/// Temporarily sets the starting speed, in units of steps per 10000 seconds.
///
/// This function sends a "Set starting speed" command to the Tic.  For more
/// information, see tic_settings_set_starting_speed().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_starting_speed(tic_handle *, uint32_t starting_speed);

/// Temporarily sets the maximum acceleration, in units of steps per second per
/// 100 seconds.
///
/// This function sends a "Set max acceleration" command to the Tic.  For more
/// information, see tic_settings_set_max_accel().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_max_accel(tic_handle *, uint32_t max_accel);

/// Temporarily sets the maximum deceleration.
///
/// This function sends a "Set max deceleration" command to the Tic.  For more
/// information, see tic_settings_set_max_decel().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_max_decel(tic_handle *, uint32_t max_decel);

/// Temporarily sets the stepper motor's step mode, which defines how many
/// microsteps correspond to one full step.
///
/// The step_mode argument should be one of the TIC_STEP_MODE_* macros.
///
/// This function sends a "Set step mode" command to the Tic.  For more
/// information, see the Tic user's guide.
///
/// See also tic_settings_set_step_mode() and tic_variables_get_step_mode().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_step_mode(tic_handle *, uint8_t step_mode);

/// Temporarily sets the stepper motor coil current limit in milliamps.
///
/// This function sends a "Set current limit" command to the Tic.  For more
/// information, see the Tic user's guide.
///
/// To set the current limit permanently, see tic_settings_set_current_limit().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_current_limit(tic_handle *, uint32_t current_limit);

/// Temporarily sets the stepper motor coil current limit code.
///
/// This function sends a "Set current limit" command to the Tic.  For more
/// information, see the Tic user's guide.
///
/// To calculate what code to use, see tic_current_limit_ma_to_code().
///
/// To set the current limit permanently, see tic_settings_set_current_limit_code().
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_current_limit_code(tic_handle *, uint8_t current_limit);

/// Temporarily sets the stepper motor decay mode.
///
/// This function sends a "Set decay mode" command to the Tic.  This decay mode
/// will stay in effect until the Tic is powered off, reset, or reinitialized,
/// or another "Set decay mode" command is issued.  To set the decay mode
/// permanently, see tic_settings_set_decay_mode().
///
/// Different Tic products have different levels of support for this command:
///
/// - Tic T825 and Tic N825: This command is supported,, and the argument
///   should be one of the TIC_DECAY_MODE_T825_* macros.
/// - Tic T834: This command is supported, and the argument should be one of the
///   TIC_DECAY_MODE_T834_* macros.
/// - Tic T500 and Tic T249: This command is supported, but there is only one
///   decay mode (0), so it is not useful.
/// - Tic 36v4: This command is not supported, and this
///   function will return an error instead of attempting to send the command
///   to the device.
///
/// Invalid \p decay_mode arguments will be changed to 0 by the firmware.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_decay_mode(tic_handle *, uint8_t decay_mode);

/// Temporarily sets the AGC mode.
///
/// The mode argument should be TIC_AGC_MODE_OFF, TIC_AGC_MODE_ON, or
/// TIC_AGC_MODE_ACTIVE_OFF.
///
/// This function sends a "Set AGC option" command to the Tic.  This option
/// will stay in effect until the Tic is powered off, reset, or reinitialized,
/// or another "Set AGC option" command changes it.  If you want a permanent
/// effect, see tic_settings_set_agc_mode().
///
/// This command is only valid for the Tic T249.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_agc_mode(tic_handle *, uint8_t mode);

/// Temporarily sets the AGC bottom current limit.
///
/// The limit argument should be one of:
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_45
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_50
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_55
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_60
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_65
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_70
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_75
/// - TIC_AGC_BOTTOM_CURRENT_LIMIT_80
///
/// This function sends a "Set AGC option" command to the Tic.  This option
/// will stay in effect until the Tic is powered off, reset, or reinitialized,
/// or another "Set AGC option" command changes it.  If you want a permanent
/// effect, see tic_settings_set_agc_bottom_current_limit().
///
/// This command is only valid for the Tic T249.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_agc_bottom_current_limit(tic_handle *, uint8_t limit);

/// Sets the AGC current boost steps.
///
/// The steps argument should be one of:
/// - TIC_AGC_CURRENT_BOOST_STEPS_5
/// - TIC_AGC_CURRENT_BOOST_STEPS_7
/// - TIC_AGC_CURRENT_BOOST_STEPS_9
/// - TIC_AGC_CURRENT_BOOST_STEPS_11
///
/// This function sends a "Set AGC option" command to the Tic.  This option
/// will stay in effect until the Tic is powered off, reset, or reinitialized,
/// or another "Set AGC option" command changes it.  If you want a permanent
/// effect, see tic_settings_set_agc_current_boost_steps().
///
/// This command is only valid for the Tic T249.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_agc_current_boost_steps(tic_handle *, uint8_t steps);

/// Temporarily sets the AGC frequency limit.
///
/// The limit argument should be one of:
/// - TIC_AGC_FREQUENCY_LIMIT_OFF
/// - TIC_AGC_FREQUENCY_LIMIT_225
/// - TIC_AGC_FREQUENCY_LIMIT_450
/// - TIC_AGC_FREQUENCY_LIMIT_675
///
/// This function sends a "Set AGC option" command to the Tic.  This option
/// will stay in effect until the Tic is powered off, reset, or reinitialized,
/// or another "Set AGC option" command changes it.  If you want a permanent
/// effect, see tic_settings_set_agc_frequency_limit().
///
/// This command is only valid for the Tic T249.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_agc_frequency_limit(tic_handle *, uint8_t limit);

/// Reads all of the Tic's status variables and returns them as an object.
///
/// This function sends a get variables command or a get variables and clear
/// errors occurred command to the Tic.
///
/// The variables parameter should be a non-null pointer to a tic_variables
/// pointer, which will receive a pointer to a new variables object if and only
/// if this function is successful.  The caller must free the variables later by
/// calling tic_variables_free().
///
/// To read information from the variables object, see the tic_variables_get_*
/// functions.
///
/// The clear_errors_occurred option should be true if you want to clear the
/// bits in the device's Errors Occurred variable as a side effect.
TIC_API TIC_WARN_UNUSED
tic_error * tic_get_variables(tic_handle *, tic_variables ** variables,
  bool clear_errors_occurred);

/// Reads all of the Tic's non-volatile settings and returns them as an object.
///
/// The settings parameter should be a non-null pointer to a tic_settings
/// pointer, which will receive a pointer to a new settings object if and only
/// if this function is successful.  The caller must free the settings later by
/// calling tic_settings_free().
///
/// To access fields in the variables, see the tic_settings_* functions.
TIC_API TIC_WARN_UNUSED
tic_error * tic_get_settings(tic_handle *, tic_settings ** settings);

/// Writes all of the Tic's non-volatile settings.
///
/// Internally, this function copies the settings and calls tic_settings_fix()
/// to make sure that the settings will be valid when they are written to the
/// device.  If you want to get warnings about what was changed, you should call
/// tic_settings_fix() yourself beforehand.
///
/// After calling this function, to make the settings actually take effect, you
/// should call tic_reinitialize().
///
/// Be careful not to call this command in a fast loop to avoid wearing out the
/// Tic's EEPROM, which is rated for only 100,000 write cycles.
TIC_API TIC_WARN_UNUSED
tic_error * tic_set_settings(tic_handle *, const tic_settings *);

/// Resets the Tic's settings to their factory default values.
TIC_API TIC_WARN_UNUSED
tic_error * tic_restore_defaults(tic_handle * handle);

/// Causes the Tic to reload all of its settings from EEPROM and make them take
/// effect.  See also tic_reset().
TIC_API TIC_WARN_UNUSED
tic_error * tic_reinitialize(tic_handle * handle);

/// Makes the Tic start its bootloader, disconnecting from the computer and
/// reappearing as a different device.
TIC_API TIC_WARN_UNUSED
tic_error * tic_start_bootloader(tic_handle * handle);

/// \cond
TIC_API TIC_WARN_UNUSED
tic_error * tic_get_debug_data(tic_handle *, uint8_t * data, size_t * size);
/// \endcond


//// Current limits

/// Gets the maximum allowed current limit setting for the specified Tic
/// product (one the TIC_PRODUCT_* macros), in milliamps.
///
/// If you try to set a current limit higher than this, the Tic's firmware will
/// adjust it down because of of hardware limitations.
///
/// This function returns 9095 for the Tic 36v4, but to actually succeed in
/// setting current limits above 4000, you must enable unrestricted current
/// limits.  See tic_settings_set_hp_enable_unrestricted_current_limits().
TIC_API
uint32_t tic_get_max_allowed_current(uint8_t product);

/// Gets a list of the recommended current limit codes for the specified
/// product.  They will be in ascending order by current limit in milliamps.
TIC_API
const uint8_t * tic_get_recommended_current_limit_codes(
  uint8_t product, size_t * code_count);

// Converts current limit codes to milliamps for the specified product.
//
// The code argument should be a current limit code, but it doesn't have to be a
// recommended one.
//
// See also tic_current_limit_ma_to_code().
TIC_API
uint32_t tic_current_limit_code_to_ma(uint8_t product, uint8_t code);

// Converts a current limit value in milliamps into a recommended max current
// code.
//
// Note that this function only returns codes that are in the recommended set, a
// subset of the codes supported by the device.
//
// The raw argument should be a raw current limit value.
//
// See also tic_current_limit_code_to_ma().
TIC_API
uint8_t tic_current_limit_ma_to_code(uint8_t product, uint32_t ma);

#ifdef __cplusplus
}
#endif
