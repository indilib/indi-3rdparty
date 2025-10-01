// Functions for communicating with Tic devices over USB.

#include "tic_internal.h"

struct tic_handle
{
  libusbp_generic_handle * usb_handle;
  tic_device * device;
  char * cached_firmware_version_string;
};

tic_error * tic_handle_open(const tic_device * device, tic_handle ** handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle output pointer is null.");
  }

  *handle = NULL;

  if (device == NULL)
  {
    return tic_error_create("Device is null.");
  }

  tic_error * error = NULL;

  if (error == NULL)
  {
    uint16_t version = tic_device_get_firmware_version(device);
    uint8_t version_major = version >> 8;
    if (version_major > TIC_FIRMWARE_VERSION_MAJOR_MAX)
    {
      error = tic_error_create(
        "The device has new firmware that is not supported by this software.  "
        "Try using the latest version of this software from " DOCUMENTATION_URL);
    }
  }

  tic_handle * new_handle = NULL;
  if (error == NULL)
  {
    new_handle = calloc(1, sizeof(tic_handle));
    if (new_handle == NULL)
    {
      error = &tic_error_no_memory;
    }
  }

  if (error == NULL)
  {
    error = tic_device_copy(device, &new_handle->device);
  }


  if (error == NULL)
  {
    const libusbp_generic_interface * usb_interface =
      tic_device_get_generic_interface(device);
    error = tic_usb_error(libusbp_generic_handle_open(
        usb_interface, &new_handle->usb_handle));
  }

  if (error == NULL)
  {
    // Set a timeout for all control transfers to prevent the program from
    // hanging indefinitely.  Want it to be at least 1500 ms because that is how
    // long the Tic might take to respond after restoring its settings to their
    // defaults.
    error = tic_usb_error(libusbp_generic_handle_set_timeout(
        new_handle->usb_handle, 0, 1600));
  }

  if (error == NULL)
  {
    // Success.  Pass the handle to the caller.
    *handle = new_handle;
    new_handle = NULL;
  }

  tic_handle_close(new_handle);

  return error;
}

void tic_handle_close(tic_handle * handle)
{
  if (handle != NULL)
  {
    libusbp_generic_handle_close(handle->usb_handle);
    tic_device_free(handle->device);
    free(handle->cached_firmware_version_string);
    free(handle);
  }
}

const tic_device * tic_handle_get_device(const tic_handle * handle)
{
  if (handle == NULL) { return NULL; }
  return handle->device;
}

const char * tic_get_firmware_version_string(tic_handle * handle)
{
  if (handle == NULL) { return ""; }

  if (handle->cached_firmware_version_string != NULL)
  {
    return handle->cached_firmware_version_string;
  }

  // Allocate memory for the string.
  // - Initial part, e.g. "99.99": up to 5 bytes
  // - Modification string: up to 127 bytes
  // - Null terminator: 1 byte
  char * new_string = malloc(133);
  if (new_string == NULL)
  {
    return "";
  }

  size_t index = 0;

  // Format the firmware version number and put it in the string.
  uint16_t version_bcd = tic_device_get_firmware_version(handle->device);

  if (version_bcd & 0xF000)
  {
    new_string[index++] = '0' + (version_bcd >> 12 & 0xF);
  }
  new_string[index++] = '0' + (version_bcd >> 8 & 0xF);
  new_string[index++] = '.';
  new_string[index++] = '0' + (version_bcd >> 4 & 0xF);
  new_string[index++] = '0' + (version_bcd >> 0 & 0xF);

  // Get the firmware modification string from the device.
  size_t transferred = 0;
  uint8_t buffer[256];
  libusbp_error * usb_error = libusbp_control_transfer(handle->usb_handle,
    0x80, USB_REQUEST_GET_DESCRIPTOR,
    (USB_DESCRIPTOR_TYPE_STRING << 8) | TIC_FIRMWARE_MODIFICATION_STRING_INDEX,
    0,
    buffer, sizeof(buffer), &transferred);
  if (usb_error)
  {
    // Let's make this be a non-fatal error because it's not so important.
    // Just add a question mark so we can tell if something is wrong.
    new_string[index++] = '0';
  }

  // Ignore the modification string if it is just a dash.
  if (transferred == 4 && buffer[2] == '-')
  {
    transferred = 0;
  }

  // Add the modification string to the firmware version string.
  for (size_t i = 2; i < transferred; i += 2)
  {
    new_string[index++] = buffer[i];
  }

  new_string[index] = 0;

  handle->cached_firmware_version_string = new_string;

  return new_string;
}

tic_error * tic_set_target_position(tic_handle * handle, int32_t position)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)position & 0xFFFF;
  uint16_t wIndex = (uint32_t)position >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_TARGET_POSITION, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the target position.");
  }

  return error;
}

tic_error * tic_set_target_velocity(tic_handle * handle, int32_t velocity)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)velocity & 0xFFFF;
  uint16_t wIndex = (uint32_t)velocity >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_TARGET_VELOCITY, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the target velocity.");
  }

  return error;
}

tic_error * tic_halt_and_set_position(tic_handle * handle, int32_t position)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)position & 0xFFFF;
  uint16_t wIndex = (uint32_t)position >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_HALT_AND_SET_POSITION, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error halting and setting the position.");
  }

  return error;
}

tic_error * tic_halt_and_hold(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_HALT_AND_HOLD, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error halting.");
  }

  return error;
}

tic_error * tic_go_home(tic_handle * handle, uint8_t direction)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_GO_HOME, direction, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error sending the 'Go home' command.");
  }

  return error;
}

tic_error * tic_reset_command_timeout(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_RESET_COMMAND_TIMEOUT, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error resetting the command timeout.");
  }

  return error;
}

tic_error * tic_deenergize(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_DEENERGIZE, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error deenergizing.");
  }

  return error;
}

tic_error * tic_energize(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_ENERGIZE, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error energizing.");
  }

  return error;
}

tic_error * tic_exit_safe_start(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_EXIT_SAFE_START, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error exiting safe start.");
  }

  return error;
}

tic_error * tic_enter_safe_start(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_ENTER_SAFE_START, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error entering safe start.");
  }

  return error;
}

tic_error * tic_reset(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_RESET, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error sending the Reset command.");
  }

  return error;
}

tic_error * tic_clear_driver_error(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_CLEAR_DRIVER_ERROR, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error clearing the driver error.");
  }

  return error;
}

tic_error * tic_set_max_speed(tic_handle * handle, uint32_t max_speed)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)max_speed & 0xFFFF;
  uint16_t wIndex = (uint32_t)max_speed >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_MAX_SPEED, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the maximum speed.");
  }

  return error;
}

tic_error * tic_set_starting_speed(tic_handle * handle, uint32_t starting_speed)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)starting_speed & 0xFFFF;
  uint16_t wIndex = (uint32_t)starting_speed >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_STARTING_SPEED, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the starting speed.");
  }

  return error;
}

tic_error * tic_set_max_accel(tic_handle * handle, uint32_t max_accel)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)max_accel & 0xFFFF;
  uint16_t wIndex = (uint32_t)max_accel >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_MAX_ACCEL, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the maximum acceleration.");
  }

  return error;
}

tic_error * tic_set_max_decel(tic_handle * handle, uint32_t max_decel)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = (uint32_t)max_decel & 0xFFFF;
  uint16_t wIndex = (uint32_t)max_decel >> 16 & 0xFFFF;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_MAX_DECEL, wValue, wIndex, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the maximum deceleration.");
  }

  return error;
}

tic_error * tic_set_step_mode(tic_handle * handle, uint8_t step_mode)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint16_t wValue = step_mode;
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_STEP_MODE, wValue, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the step mode.");
  }

  return error;
}

tic_error * tic_set_current_limit(tic_handle * handle, uint32_t current_limit)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  const tic_device * device = tic_handle_get_device(handle);
  uint8_t product = tic_device_get_product(device);
  uint8_t code = tic_current_limit_ma_to_code(product, current_limit);

  return tic_set_current_limit_code(handle, code);
}

tic_error * tic_set_current_limit_code(tic_handle * handle, uint8_t code)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_CURRENT_LIMIT, code, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the current limit.");
  }

  return error;
}

tic_error * tic_set_decay_mode(tic_handle * handle, uint8_t decay_mode)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  uint8_t product = tic_device_get_product(tic_handle_get_device(handle));
  switch (product)
  {
  case TIC_PRODUCT_T825:
  case TIC_PRODUCT_N825:
  case TIC_PRODUCT_T834:
    // This Tic supports the "Set decay mode" command and it is useful.
    break;
  case TIC_PRODUCT_T500:
  case TIC_PRODUCT_T249:
    // This Tic supports the "Set decay mode" command but there is only
    // one decay mode (0) so it is not useful.  But let's send the command
    // anyway because previous versions of the library did, and we do not
    // want to have a breaking change.
    break;
  default:
    // This Tic does not support the "Set decay mode" command.
    error = tic_error_create(
      "This Tic product does not support the \"Set decay mode\" command.");
    break;
  }

  if (error == NULL)
  {
    uint16_t wValue = decay_mode;
    error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
      0x40, TIC_CMD_SET_DECAY_MODE, wValue, 0, NULL, 0, NULL));
  }

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting the decay mode.");
  }

  return error;
}

static tic_error * tic_set_agc_option(tic_handle * handle,
  uint8_t option, uint8_t value)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  const tic_device * device = tic_handle_get_device(handle);
  uint8_t product = tic_device_get_product(device);
  if (product != TIC_PRODUCT_T249)
  {
    return tic_error_create(
      "This Tic does not support AGC or the commands to configure it.");
  }

  tic_error * error = NULL;

  uint16_t wValue = ((option & 0x07) << 4) | (value & 0x0F);
  error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_AGC_OPTION, wValue, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error setting an AGC option (%d,%d).", option, value);
  }

  return error;
}

tic_error * tic_set_agc_mode(tic_handle * handle, uint8_t mode)
{
  return tic_set_agc_option(handle, TIC_AGC_OPTION_MODE, mode);
}

tic_error * tic_set_agc_bottom_current_limit(tic_handle * handle, uint8_t limit)
{
  return tic_set_agc_option(handle, TIC_AGC_OPTION_BOTTOM_CURRENT_LIMIT, limit);
}

tic_error * tic_set_agc_current_boost_steps(tic_handle * handle, uint8_t steps)
{
  return tic_set_agc_option(handle, TIC_AGC_OPTION_CURRENT_BOOST_STEPS, steps);
}

tic_error * tic_set_agc_frequency_limit(tic_handle * handle, uint8_t limit)
{
  return tic_set_agc_option(handle, TIC_AGC_OPTION_FREQUENCY_LIMIT, limit);
}

tic_error * tic_set_setting_byte(tic_handle * handle,
  uint8_t address, uint8_t byte)
{
  assert(handle != NULL);

  tic_error * error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_SET_SETTING, byte, address, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error applying settings.");
  }
  return error;
}

tic_error * tic_set_setting_segment(tic_handle * handle,
  uint8_t address, size_t length, const uint8_t * input)
{
  tic_error * error = NULL;
  for (uint8_t i = 0; i < length && error == NULL; i++)
  {
    error = tic_set_setting_byte(handle, address + i, input[i]);
  }
  return error;
}

tic_error * tic_get_setting_segment(tic_handle * handle,
  uint8_t index, size_t length, uint8_t * output)
{
  assert(handle != NULL);
  assert(output != NULL);
  assert(length && length <= TIC_MAX_USB_RESPONSE_SIZE);

  size_t transferred;
  tic_error * error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0xC0, TIC_CMD_GET_SETTING, 0, index, output, length, &transferred));
  if (error != NULL)
  {
    return error;
  }

  if (transferred != length)
  {
    return tic_error_create(
      "Failed to read settings.  Expected %u bytes, got %u.\n",
      (unsigned int)length, (unsigned int)transferred);
  }

  return NULL;
}

tic_error * tic_get_variable_segment(tic_handle * handle,
  size_t index, size_t length, uint8_t * output,
  bool clear_errors_occurred)
{
  assert(handle != NULL);
  assert(output != NULL);
  assert(length && length <= TIC_MAX_USB_RESPONSE_SIZE);

  uint8_t cmd = TIC_CMD_GET_VARIABLE;
  if (clear_errors_occurred)
  {
    cmd = TIC_CMD_GET_VARIABLE_AND_CLEAR_ERRORS_OCCURRED;
  }
  size_t transferred;
  tic_error * error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0xC0, cmd, 0, index, output, length, &transferred));
  if (error != NULL)
  {
    return error;
  }

  if (transferred != length)
  {
    return tic_error_create(
      "Failed to read variables with command 0x%x.  Expected %u bytes, got %u.\n",
      cmd, (unsigned int)length, (unsigned int)transferred);
  }

  return NULL;
}


tic_error * tic_restore_defaults(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  if (error == NULL)
  {
    error = tic_set_setting_byte(handle, TIC_SETTING_NOT_INITIALIZED, 1);
  }

  if (error == NULL)
  {
    error = tic_reinitialize(handle);
  }

  // The request returns before the settings are actually initialized.
  // Wait until the device succeeds in reinitializing its settings.
  if (error == NULL)
  {
    time_t start = time(0);
    while (true)
    {
      usleep(10000);

      uint8_t not_initialized;
      error = tic_get_setting_segment(handle, TIC_SETTING_NOT_INITIALIZED,
        1, &not_initialized);
      if (error != NULL)
      {
        // Communication error
        break;
      }

      if (!not_initialized)
      {
        // Success
        break;
      }

      if (difftime(time(0), start) >= 3)
      {
        // Time out after 2 to 3 seconds.
        error = tic_error_create("The device took too long to finish.");
        break;
      }
    }
  }

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error restoring the default settings.");
  }

  return error;
}

tic_error * tic_reinitialize(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_REINITIALIZE, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error reinitializing the device.");
  }

  return error;
}

tic_error * tic_start_bootloader(tic_handle * handle)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = tic_usb_error(libusbp_control_transfer(handle->usb_handle,
    0x40, TIC_CMD_START_BOOTLOADER, 0, 0, NULL, 0, NULL));

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error starting the bootloader.");
  }

  return error;
}

tic_error * tic_get_debug_data(tic_handle * handle, uint8_t * data, size_t * size)
{
  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  if (data == NULL)
  {
    return tic_error_create("Data output pointer is null.");
  }

  if (size == NULL)
  {
    return tic_error_create("Size output pointer is null.");
  }

  size_t transferred;
  libusbp_error * usb_error = libusbp_control_transfer(handle->usb_handle,
    0xC0, TIC_CMD_GET_DEBUG_DATA, 0, 0, data, *size, &transferred);
  if (usb_error)
  {
    *size = 0;
    return tic_usb_error(usb_error);
  }

  *size = transferred;

  return NULL;
}
