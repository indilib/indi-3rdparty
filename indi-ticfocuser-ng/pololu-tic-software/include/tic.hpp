// Copyright (C) Pololu Corporation.  See www.pololu.com for details.

/// \file tic.hpp
///
/// This file provides the C++ API for libpololu-tic, a library that supports
/// communciating with the Pololu Tic USB Stepper Motor Controller.  The classes
/// and functions here are just thin wrappers around the C API defined in tic.h.
///
/// Note: There are some C functions in libpololu-tic that are not wrapped here
/// because they can easily be called from C++.

#pragma once

#include "tic.h"
#include <cstddef>
#include <utility>
#include <memory>
#include <string>
#include <vector>

// Display a nice error if C++11 is not enabled (e.g. --std=gnu++11).
#if (!defined(__cplusplus) || (__cplusplus < 201103L)) && \
    !defined(__GXX_EXPERIMENTAL_CXX0X__) && !defined(_MSC_VER)
#error This header requires features from C++11.
#endif

namespace tic
{
  /// \cond
  inline void throw_if_needed(tic_error * err);
  /// \endcond

  /// Wrapper for tic_error_free().
  inline void pointer_free(tic_error * p) noexcept
  {
    tic_error_free(p);
  }

  /// Wrapper for tic_error_copy().
  inline tic_error * pointer_copy(const tic_error * p) noexcept
  {
    return tic_error_copy(p);
  }

  /// Wrapper for tic_variables_free().
  inline void pointer_free(tic_variables * p) noexcept
  {
    tic_variables_free(p);
  }

  /// Wrapper for tic_variables_copy().
  inline tic_variables * pointer_copy(const tic_variables * p)
  {
    tic_variables * copy;
    throw_if_needed(tic_variables_copy(p, &copy));
    return copy;
  }

  /// Wrapper for tic_settings_free().
  inline void pointer_free(tic_settings * p) noexcept
  {
    tic_settings_free(p);
  }

  /// Wrapper for tic_settings_copy().
  inline tic_settings * pointer_copy(const tic_settings * p)
  {
    tic_settings * copy;
    throw_if_needed(tic_settings_copy(p, &copy));
    return copy;
  }

  /// Wrapper for tic_device_free().
  inline void pointer_free(tic_device * p) noexcept
  {
    tic_device_free(p);
  }

  /// Wrapper for tic_device_copy().
  inline tic_device * pointer_copy(const tic_device * p)
  {
    tic_device * copy;
    throw_if_needed(tic_device_copy(p, &copy));
    return copy;
  }

  /// Wrapper for tic_handle_close().
  inline void pointer_free(tic_handle * p) noexcept
  {
    tic_handle_close(p);
  }

  /// This class is not part of the public API of the library and you should
  /// not use it directly, but you can use the public methods it provides to
  /// the classes that inherit from it.
  template<class T>
  class unique_pointer_wrapper
  {
  public:
    /// Constructor that takes a pointer.
    explicit unique_pointer_wrapper(T * p = NULL) noexcept : pointer(p)
    {
    }

    /// Move constructor.
    unique_pointer_wrapper(unique_pointer_wrapper && other) noexcept
    {
      pointer = other.pointer_release();
    }

    /// Move assignment operator.
    unique_pointer_wrapper & operator=(unique_pointer_wrapper && other) noexcept
    {
      pointer_reset(other.pointer_release());
      return *this;
    }

    /// Destructor
    ~unique_pointer_wrapper() noexcept
    {
      pointer_reset();
    }

    /// Implicit conversion to bool.  Returns true if the underlying pointer is
    /// not NULL.
    ///
    /// NOTE: This kind of implicit conversion is dangerous and should not be
    /// defined in future libraries.  The type-checker will not catch code like
    /// "int x = some_object();" so it makes refactoring harder.
    operator bool() const noexcept
    {
      return pointer != NULL;
    }

    /// Returns the underlying pointer.
    T * get_pointer() const noexcept
    {
      return pointer;
    }

    /// Sets the underlying pointer to the specified value, freeing the
    /// previous pointer and taking ownership of the specified one.
    void pointer_reset(T * p = NULL) noexcept
    {
      pointer_free(pointer);
      pointer = p;
    }

    /// Releases the pointer, transferring ownership of it to the caller and
    /// resetting the underlying pointer of this object to NULL.  The caller
    /// is responsible for freeing the returned pointer if it is not NULL.
    T * pointer_release() noexcept
    {
      T * p = pointer;
      pointer = NULL;
      return p;
    }

    /// Returns a pointer to the underlying pointer.
    T ** get_pointer_to_pointer() noexcept
    {
      return &pointer;
    }

    /// Copy constructor: forbid.
    unique_pointer_wrapper(const unique_pointer_wrapper & other) = delete;

    /// Copy assignment operator: forbid.
    unique_pointer_wrapper & operator=(const unique_pointer_wrapper & other) = delete;

  protected:
    T * pointer;
  };

  /// This class is not part of the public API of the library and you should not
  /// use it directly, but you can use the public methods it provides to the
  /// classes that inherit from it.
  template <class T>
  class unique_pointer_wrapper_with_copy : public unique_pointer_wrapper<T>
  {
  public:
    /// Constructor that takes a pointer.
    explicit unique_pointer_wrapper_with_copy(T * p = NULL) noexcept
      : unique_pointer_wrapper<T>(p)
    {
    }

    /// Move constructor.
    unique_pointer_wrapper_with_copy(
      unique_pointer_wrapper_with_copy && other) = default;

    /// Copy constructor.
    unique_pointer_wrapper_with_copy(
      const unique_pointer_wrapper_with_copy & other)
      : unique_pointer_wrapper<T>()
    {
      this->pointer = pointer_copy(other.pointer);
    }

    /// Copy assignment operator.
    unique_pointer_wrapper_with_copy & operator=(
      const unique_pointer_wrapper_with_copy & other)
    {
      this->pointer_reset(pointer_copy(other.pointer));
      return *this;
    }

    /// Move assignment operator.
    unique_pointer_wrapper_with_copy & operator=(
      unique_pointer_wrapper_with_copy && other) = default;
  };

  /// Represents an error from a library call.
  class error : public unique_pointer_wrapper_with_copy<tic_error>, public std::exception
  {
  public:
    /// Constructor that takes a pointer from the C API.
    explicit error(tic_error * p = NULL) noexcept :
      unique_pointer_wrapper_with_copy(p)
    {
    }

    /// Similar to message(), but returns a C string.  The returned string will
    /// be valid at least until this object is destroyed or modified.
    virtual const char * what() const noexcept
    {
      return tic_error_get_message(pointer);
    }

    /// Returns an English-language ASCII-encoded string describing the error.
    /// The string consists of one or more complete sentences.
    std::string message() const
    {
      return tic_error_get_message(pointer);
    }

    /// Returns true if the error has specified error code.  The error codes are
    /// listed in the ::tic_error_code enum.
    bool has_code(uint32_t error_code) const noexcept
    {
      return tic_error_has_code(pointer, error_code);
    }
  };

  /// \cond
  inline void throw_if_needed(tic_error * err)
  {
    if (err != NULL) { throw error(err); }
  }
  /// \endcond

  /// Represets the settings for a Tic.  This object just stores plain old data;
  /// it does not have any pointers or handles for other resources.
  ///
  /// This class does not currently have functions for reading and writing most
  /// settings.  You will have to use the tic_settings_* functions from the C
  /// API defined in tic.h to read and write the settings in the object.  For
  /// example:
  ///
  ///     tic::settings settings;
  ///     tic_settings_set_serial_device_number(settings.get_pointer(), 11);
  ///
  /// The GUI code has more examples of how to do this.
  class settings : public unique_pointer_wrapper_with_copy<tic_settings>
  {
  public:
    /// Constructor that takes a pointer from the C API.
    explicit settings(tic_settings * p = NULL) noexcept :
      unique_pointer_wrapper_with_copy(p)
    {
    }

    /// Wrapper for tic_settings_create().
    static settings create()
    {
      tic_settings * p;
      throw_if_needed(tic_settings_create(&p));
      return settings(p);
    }

    /// Wrapper for tic_settings_fill_with_defaults().
    void fill_with_defaults()
    {
      tic_settings_fill_with_defaults(pointer);
    }

    /// Wrapper for tic_settings_fix().
    ///
    /// If a non-NULL warnings pointer is provided, and this function does not
    /// throw an exception, the string it points to will be overridden with an
    /// empty string or a string with English warnings.
    void fix(std::string * warnings = NULL)
    {
      char * cstr = NULL;
      char ** cstr_pointer = warnings ? &cstr : NULL;
      throw_if_needed(tic_settings_fix(pointer, cstr_pointer));
      if (warnings) { *warnings = std::string(cstr); }
    }

    /// Wrapper for tic_settings_to_string().
    std::string to_string() const
    {
      char * str;
      throw_if_needed(tic_settings_to_string(pointer, &str));
      return std::string(str);
    }

    /// Wrapper for tic_settings_read_from_string.
    static settings read_from_string(const std::string & settings_string)
    {
      settings r;
      throw_if_needed(tic_settings_read_from_string(
          settings_string.c_str(), r.get_pointer_to_pointer()));
      return r;
    }

    /// Wrapper for tic_settings_set_product().
    void set_product(uint8_t product)
    {
      tic_settings_set_product(pointer, product);
    }

    /// Wrapper for tic_settings_get_product().
    uint8_t get_product() const
    {
      return tic_settings_get_product(pointer);
    }
  };

  /// Represents the variables read from a Tic.  This object just stores plain
  /// old data; it does not have any pointer or handles for other resources.
  class variables : public unique_pointer_wrapper_with_copy<tic_variables>
  {
  public:
    /// Constructor that takes a pointer from the C API.
    explicit variables(tic_variables * p = NULL) noexcept :
      unique_pointer_wrapper_with_copy(p)
    {
    }

    /// Wrapper for tic_variables_get_operation_state().
    uint8_t get_operation_state() const noexcept
    {
      return tic_variables_get_operation_state(pointer);
    }

    /// Wrapper for tic_variables_get_energized().
    bool get_energized() const noexcept
    {
      return tic_variables_get_energized(pointer);
    }

    /// Wrapper for tic_variables_get_position_uncertain().
    bool get_position_uncertain() const noexcept
    {
      return tic_variables_get_position_uncertain(pointer);
    }

    /// Wrapper for tic_variables_get_forward_limit_active().
    bool get_forward_limit_active() const noexcept
    {
      return tic_variables_get_forward_limit_active(pointer);
    }

    /// Wrapper for tic_variables_get_reverse_limit_active().
    bool get_reverse_limit_active() const noexcept
    {
      return tic_variables_get_reverse_limit_active(pointer);
    }

    /// Wrapper for tic_variables_get_homing_active().
    bool get_homing_active() const noexcept
    {
      return tic_variables_get_homing_active(pointer);
    }

    /// Wrapper for tic_variables_get_error_status().
    uint16_t get_error_status() const noexcept
    {
      return tic_variables_get_error_status(pointer);
    }

    /// Wrapper for tic_variables_get_errors_occurred().
    uint32_t get_errors_occurred() const noexcept
    {
      return tic_variables_get_errors_occurred(pointer);
    }

    /// Wrapper for tic_variables_get_planning_mode().
    uint8_t get_planning_mode() const noexcept
    {
      return tic_variables_get_planning_mode(pointer);
    }

    /// Wrapper for tic_variables_get_target_position().
    int32_t get_target_position() const noexcept
    {
      return tic_variables_get_target_position(pointer);
    }

    /// Wrapper for tic_variables_get_target_velocity().
    int32_t get_target_velocity() const noexcept
    {
      return tic_variables_get_target_velocity(pointer);
    }

    /// Wrapper for tic_variables_get_max_speed().
    uint32_t get_max_speed() const noexcept
    {
      return tic_variables_get_max_speed(pointer);
    }

    /// Wrapper for tic_variables_get_starting_speed().
    uint32_t get_starting_speed() const noexcept
    {
      return tic_variables_get_starting_speed(pointer);
    }

    /// Wrapper for tic_variables_get_max_accel().
    uint32_t get_max_accel() const noexcept
    {
      return tic_variables_get_max_accel(pointer);
    }

    /// Wrapper for tic_variables_get_max_decel().
    uint32_t get_max_decel() const noexcept
    {
      return tic_variables_get_max_decel(pointer);
    }

    /// Wrapper for tic_variables_get_current_position().
    int32_t get_current_position() const noexcept
    {
      return tic_variables_get_current_position(pointer);
    }

    /// Wrapper for tic_variables_get_current_velocity().
    int32_t get_current_velocity() const noexcept
    {
      return tic_variables_get_current_velocity(pointer);
    }

    /// Wrapper for tic_variables_get_acting_target_position().
    int32_t get_acting_target_position() const noexcept
    {
      return tic_variables_get_acting_target_position(pointer);
    }

    /// Wrapper for tic_variables_get_time_since_last_step().
    uint32_t get_time_since_last_step() const noexcept
    {
      return tic_variables_get_time_since_last_step(pointer);
    }

    /// Wrapper for tic_variables_get_device_reset().
    uint8_t get_device_reset() const noexcept
    {
      return tic_variables_get_device_reset(pointer);
    }

    /// Wrapper for tic_variables_get_vin_voltage().
    uint32_t get_vin_voltage() const noexcept
    {
      return tic_variables_get_vin_voltage(pointer);
    }

    /// Wrapper for tic_variables_get_up_time().
    uint32_t get_up_time() const noexcept
    {
      return tic_variables_get_up_time(pointer);
    }

    /// Wrapper for tic_variables_get_encoder_position().
    int32_t get_encoder_position() const noexcept
    {
      return tic_variables_get_encoder_position(pointer);
    }

    /// Wrapper for tic_variables_get_rc_pulse_width().
    uint16_t get_rc_pulse_width() const noexcept
    {
      return tic_variables_get_rc_pulse_width(pointer);
    }

    /// Wrapper for tic_variables_get_step_mode().
    uint8_t get_step_mode() const noexcept
    {
      return tic_variables_get_step_mode(pointer);
    }

    /// Wrapper for tic_variables_get_current_limit().
    uint32_t get_current_limit() const noexcept
    {
      return tic_variables_get_current_limit(pointer);
    }

    uint8_t get_current_limit_code() const noexcept
    {
      return tic_variables_get_current_limit_code(pointer);
    }

    /// Wrapper for tic_variables_get_decay_mode().
    uint8_t get_decay_mode() const noexcept
    {
      return tic_variables_get_decay_mode(pointer);
    }

    /// Wrapper for tic_variables_get_input_state().
    uint8_t get_input_state() const noexcept
    {
      return tic_variables_get_input_state(pointer);
    }

    /// Wrapper for tic_variables_get_input_after_averaging().
    uint16_t get_input_after_averaging() const noexcept
    {
      return tic_variables_get_input_after_averaging(pointer);
    }

    /// Wrapper for tic_variables_get_input_after_hysteresis().
    uint16_t get_input_after_hysteresis() const noexcept
    {
      return tic_variables_get_input_after_hysteresis(pointer);
    }

    /// Wrapper for tic_variables_get_input_before_scaling().
    uint16_t get_input_before_scaling(const settings & settings) const noexcept
    {
      return tic_variables_get_input_before_scaling(pointer, settings.get_pointer());
    }

    /// Wrapper for tic_variables_get_input_after_scaling().
    int32_t get_input_after_scaling() const noexcept
    {
      return tic_variables_get_input_after_scaling(pointer);
    }

    /// Wrapper for tic_variables_get_last_motor_driver_error().
    uint8_t get_last_motor_driver_error() const noexcept
    {
      return tic_variables_get_last_motor_driver_error(pointer);
    }

    /// Wrapper for tic_variables_get_agc_mode().
    uint8_t get_agc_mode() const noexcept
    {
      return tic_variables_get_agc_mode(pointer);
    }

    /// Wrapper for tic_variables_get_agc_bottom_current_limit().
    uint8_t get_agc_bottom_current_limit() const noexcept
    {
      return tic_variables_get_agc_bottom_current_limit(pointer);
    }

    /// Wrapper for tic_variables_get_agc_current_boost_steps().
    uint8_t get_agc_current_boost_steps() const noexcept
    {
      return tic_variables_get_agc_current_boost_steps(pointer);
    }

    /// Wrapper for tic_variables_get_agc_frequency_limit().
    uint8_t get_agc_frequency_limit() const noexcept
    {
      return tic_variables_get_agc_frequency_limit(pointer);
    }

    /// Wrapper for tic_variables_get_analog_reading().
    uint16_t get_analog_reading(uint8_t pin) const noexcept
    {
      return tic_variables_get_analog_reading(pointer, pin);
    }

    /// Wrapper for tic_variables_get_digital_reading().
    bool get_digital_reading(uint8_t pin) const noexcept
    {
      return tic_variables_get_digital_reading(pointer, pin);
    }

    /// Wrapper for tic_variables_get_pin_state().
    uint8_t get_pin_state(uint8_t pin) const noexcept
    {
      return tic_variables_get_pin_state(pointer, pin);
    }

    /// Wrapper for tic_variables_get_last_hp_driver_errors().
    uint32_t get_last_hp_driver_errors() const noexcept
    {
      return tic_variables_get_last_hp_driver_errors(pointer);
    }
  };

  /// Represents a Tic that is or was connected to the computer.  Can also be in
  /// a null state where it does not represent a device.
  class device : public unique_pointer_wrapper_with_copy<tic_device>
  {
  public:
    /// Constructor that takes a pointer from the C API.
    explicit device(tic_device * p = NULL) noexcept :
      unique_pointer_wrapper_with_copy(p)
    {
    }

    uint8_t get_product() const noexcept
    {
      return tic_device_get_product(pointer);
    }

    /// Gets the full name of the device (e.g. "Tic USB Stepper Motor Controller
    /// T825") as an ASCII-encoded string.
    /// If the device is null, returns an empty string.
    std::string get_name() const noexcept
    {
      return tic_device_get_name(pointer);
    }

    /// Gets the short name of the device (e.g. "T825") as an ASCII-encoded
    /// string.
    /// If the device is null, returns an empty string.
    std::string get_short_name() const noexcept
    {
      return tic_device_get_short_name(pointer);
    }

    /// Gets the serial number of the device as an ASCII-encoded string.
    /// If the device is null, returns an empty string.
    std::string get_serial_number() const noexcept
    {
      return tic_device_get_serial_number(pointer);
    }

    /// Get an operating system-specific identifier for this device as an
    /// ASCII-encoded string.  The string will be valid at least as long as the
    /// device object.  If the device is NULL, returns an empty string.
    std::string get_os_id() const noexcept
    {
      return tic_device_get_os_id(pointer);
    }

    /// Gets the firmware version as a binary-coded decimal number (e.g. 0x0100
    /// means "1.00").  We recommend using
    /// tic::handle::get_firmware_version_string() instead of this function if
    /// possible.
    uint16_t get_firmware_version() const noexcept
    {
      return tic_device_get_firmware_version(pointer);
    }
  };

  /// Finds all the Tic devices connected to the computer via USB and returns a
  /// list of them.
  inline std::vector<tic::device> list_connected_devices()
  {
    tic_device ** device_list;
    size_t size;
    throw_if_needed(tic_list_connected_devices(&device_list, &size));
    std::vector<device> vector;
    for (size_t i = 0; i < size; i++)
    {
      vector.push_back(device(device_list[i]));
    }
    tic_list_free(device_list);
    return vector;
  }

  /// Represents an open handle that can be used to read and write data from a
  /// device.  Can also be in a null state where it does not represent a device.
  class handle : public unique_pointer_wrapper<tic_handle>
  {
  public:
    /// Constructor that takes a pointer from the C API.  This object will free
    /// the pointer when it is destroyed.
    explicit handle(tic_handle * p = NULL) noexcept
      : unique_pointer_wrapper(p)
    {
    }

    /// Constructor that opens a handle to the specified device.
    explicit handle(const device & device)
    {
      throw_if_needed(tic_handle_open(device.get_pointer(), &pointer));
    }

    /// Closes the handle and puts this object into the null state.
    void close() noexcept
    {
      pointer_reset();
    }

    /// Wrapper for tic_handle_get_device();
    device get_device() const
    {
      return device(pointer_copy(tic_handle_get_device(pointer)));
    }

    /// Wrapper for tic_get_firmware_version_string().
    std::string get_firmware_version_string()
    {
      return tic_get_firmware_version_string(pointer);
    }

    /// Wrapper for tic_set_target_position().
    void set_target_position(int32_t position)
    {
      throw_if_needed(tic_set_target_position(pointer, position));
    }

    /// Wrapper for tic_set_target_velocity().
    void set_target_velocity(int32_t velocity)
    {
      throw_if_needed(tic_set_target_velocity(pointer, velocity));
    }

    /// Wrapper for tic_halt_and_set_position().
    void halt_and_set_position(int32_t position)
    {
      throw_if_needed(tic_halt_and_set_position(pointer, position));
    }

    /// Wrapper for tic_halt_and_hold().
    void halt_and_hold()
    {
      throw_if_needed(tic_halt_and_hold(pointer));
    }

    /// Wrapper for tic_go_home().
    void go_home(uint8_t direction)
    {
      throw_if_needed(tic_go_home(pointer, direction));
    }

    /// Wrapper for tic_reset_command_timeout().
    void reset_command_timeout()
    {
      throw_if_needed(tic_reset_command_timeout(pointer));
    }

    /// Wrapper for tic_deenergize().
    void deenergize()
    {
      throw_if_needed(tic_deenergize(pointer));
    }

    /// Wrapper for tic_energize().
    void energize()
    {
      throw_if_needed(tic_energize(pointer));
    }

    /// Wrapper for tic_exit_safe_start().
    void exit_safe_start()
    {
      throw_if_needed(tic_exit_safe_start(pointer));
    }

    /// Wrapper for tic_enter_safe_start().
    void enter_safe_start()
    {
      throw_if_needed(tic_enter_safe_start(pointer));
    }

    /// Wrapper for tic_reset().
    void reset()
    {
      throw_if_needed(tic_reset(pointer));
    }

    /// Wrapper for tic_clear_driver_error().
    void clear_driver_error()
    {
      throw_if_needed(tic_clear_driver_error(pointer));
    }

    /// Wrapper for tic_set_max_speed().
    void set_max_speed(uint32_t max_speed)
    {
      throw_if_needed(tic_set_max_speed(pointer, max_speed));
    }

    /// Wrapper for tic_set_starting_speed().
    void set_starting_speed(uint32_t starting_speed)
    {
      throw_if_needed(tic_set_starting_speed(pointer, starting_speed));
    }

    /// Wrapper for tic_set_starting_speed().
    void set_max_accel(uint32_t max_accel)
    {
      throw_if_needed(tic_set_max_accel(pointer, max_accel));
    }

    /// Wrapper for tic_set_starting_speed().
    void set_max_decel(uint32_t max_decel)
    {
      throw_if_needed(tic_set_max_decel(pointer, max_decel));
    }

    /// Wrapper for tic_set_step_mode().
    void set_step_mode(uint8_t step_mode)
    {
      throw_if_needed(tic_set_step_mode(pointer, step_mode));
    }

    /// Wrapper for tic_set_current_limit().
    void set_current_limit(uint32_t current_limit)
    {
      throw_if_needed(tic_set_current_limit(pointer, current_limit));
    }

    /// Wrapper for tic_set_current_limit_code().
    void set_current_limit_code(uint8_t code)
    {
      throw_if_needed(tic_set_current_limit_code(pointer, code));
    }

    /// Wrapper for tic_set_decay_mode().
    void set_decay_mode(uint8_t decay_mode)
    {
      throw_if_needed(tic_set_decay_mode(pointer, decay_mode));
    }

    /// Wrapper for tic_set_agc_mode().
    void set_agc_mode(uint8_t mode)
    {
      throw_if_needed(tic_set_agc_mode(pointer, mode));
    }

    /// Wrapper for tic_set_agc_bottom_current_limit().
    void set_agc_bottom_current_limit(uint8_t limit)
    {
      throw_if_needed(tic_set_agc_bottom_current_limit(pointer, limit));
    }

    /// Wrapper for tic_set_agc_current_boost_steps().
    void set_agc_current_boost_steps(uint8_t steps)
    {
      throw_if_needed(tic_set_agc_current_boost_steps(pointer, steps));
    }

    /// Wrapper for tic_set_agc_frequency_limit().
    void set_agc_frequency_limit(uint8_t limit)
    {
      throw_if_needed(tic_set_agc_frequency_limit(pointer, limit));
    }

    /// Wrapper for tic_get_variables().
    variables get_variables(bool clear_errors_occurred = false)
    {
      tic_variables * v;
      throw_if_needed(tic_get_variables(pointer, &v, clear_errors_occurred));
      return variables(v);
    }

    /// Wrapper for tic_get_settings().
    settings get_settings()
    {
      tic_settings * s;
      throw_if_needed(tic_get_settings(pointer, &s));
      return settings(s);
    }

    /// Wrapper for tic_set_settings().
    void set_settings(const settings & settings)
    {
      throw_if_needed(tic_set_settings(pointer, settings.get_pointer()));
    }

    /// Wrapper for tic_restore_defaults().
    void restore_defaults()
    {
      throw_if_needed(tic_restore_defaults(pointer));
    }

    /// Wrapper for tic_reinitialize().
    void reinitialize()
    {
      throw_if_needed(tic_reinitialize(pointer));
    }

    /// Wrapper for tic_start_bootloader().
    void start_bootloader()
    {
      throw_if_needed(tic_start_bootloader(pointer));
    }

    /// \cond
    void get_debug_data(std::vector<uint8_t> & data)
    {
      size_t size = data.size();
      throw_if_needed(tic_get_debug_data(pointer, data.data(), &size));
      data.resize(size);
    }
    /// \endcond

  };

  /// Wrapper for tic_get_recommended_current_limit_codes().
  inline const std::vector<uint8_t> get_recommended_current_limit_codes(
    uint8_t product)
  {
    size_t count;
    const uint8_t * table = tic_get_recommended_current_limit_codes(
      product, &count);
    return std::vector<uint8_t>(table, table + count);
  }

  // Wrapper for tic_current_limit_code_to_ma().
  inline uint32_t current_limit_code_to_ma(uint8_t product, uint8_t code)
  {
    return tic_current_limit_code_to_ma(product, code);
  }

  // Wrapper for tic_current_limit_ma_to_code().
  inline uint8_t current_limit_ma_to_code(uint8_t product, uint32_t ma)
  {
    return tic_current_limit_ma_to_code(product, ma);
  }
}

