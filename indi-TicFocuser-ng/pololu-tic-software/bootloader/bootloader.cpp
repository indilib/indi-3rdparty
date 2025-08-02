// Code for talking to the device's bootloader and doing firmware upgrades.
//
// This code was mostly copied from p-load:
//
//   https://github.com/pololu/p-load
//
// NOTE: It would be nice to fix the code style of this file and related files,
// and remove features we don't need.

#include "bootloader.h"
#include <cstring>

// Request codes used to talk to the bootloader.
#define REQUEST_INITIALIZE         0x80
#define REQUEST_ERASE_FLASH        0x81
#define REQUEST_WRITE_FLASH_BLOCK  0x82
#define REQUEST_GET_LAST_ERROR     0x83
#define REQUEST_CHECK_APPLICATION  0x84
#define REQUEST_READ_FLASH         0x86
#define REQUEST_SET_DEVICE_CODE    0x87
#define REQUEST_READ_EEPROM        0x88
#define REQUEST_WRITE_EEPROM       0x89
#define REQUEST_RESTART            0xFE

// Request codes used to talk to a typical native USB app.
#define REQUEST_START_BOOTLOADER  0xFF

// Error codes returned by REQUEST_ERASE_FLASH and REQUEST_GET_LAST_ERROR.
#define BOOTLOADER_ERROR_STATE                1
#define BOOTLOADER_ERROR_LENGTH               2
#define BOOTLOADER_ERROR_PROGRAMMING          3
#define BOOTLOADER_ERROR_WRITE_PROTECTION     4
#define BOOTLOADER_ERROR_VERIFICATION         5
#define BOOTLOADER_ERROR_ADDRESS_RANGE        6
#define BOOTLOADER_ERROR_ADDRESS_ORDER        7
#define BOOTLOADER_ERROR_ADDRESS_ALIGNMENT    8
#define BOOTLOADER_ERROR_WRITE                9
#define BOOTLOADER_ERROR_EEPROM_VERIFICATION 10

// Other bootloader constants
#define DEVICE_CODE_SIZE           16

static std::string bootloader_get_error_description(uint8_t error_code)
{
  switch (error_code)
  {
  case 0: return "Success.";
  case BOOTLOADER_ERROR_STATE: return "Device is not in the correct state.";
  case BOOTLOADER_ERROR_LENGTH: return "Invalid data length.";
  case BOOTLOADER_ERROR_PROGRAMMING: return "Programming error.";
  case BOOTLOADER_ERROR_WRITE_PROTECTION: return "Write protection error.";
  case BOOTLOADER_ERROR_VERIFICATION: return "Verification error.";
  case BOOTLOADER_ERROR_ADDRESS_RANGE: return "Address is not in the correct range.";
  case BOOTLOADER_ERROR_ADDRESS_ORDER: return "Address was not accessed in the correct order.";
  case BOOTLOADER_ERROR_ADDRESS_ALIGNMENT: return "Address does not have the correct alignment.";
  case BOOTLOADER_ERROR_WRITE: return "Write error.";
  case BOOTLOADER_ERROR_EEPROM_VERIFICATION: return "EEPROM verification error.";
  default: return std::string("Unknown error code: ") + std::to_string(error_code) + ".";
  }
}

const bootloader_type * bootloader_type_lookup(
  uint16_t usb_vendor_id, uint16_t usb_product_id)
{
  for (const bootloader_type & t : bootloader_types)
  {
    if (t.usb_vendor_id == usb_vendor_id && t.usb_product_id == usb_product_id)
    {
      return &t;
    }
  }
  return NULL;
}

std::vector<bootloader_instance> bootloader_list_connected_devices()
{
  // Get a list of all connected USB devices.
  std::vector<libusbp::device> devices = libusbp::list_connected_devices();

  std::vector<bootloader_instance> list;

  for (const libusbp::device & device : devices)
  {
    // Filter out things that are not bootloaders.
    const bootloader_type * type = bootloader_type_lookup(
      device.get_vendor_id(), device.get_product_id());
    if (!type) { continue; }

    // Get the generic interface object for interface 0.
    libusbp::generic_interface usb_interface;
    try
    {
      usb_interface = libusbp::generic_interface(device);
    }
    catch(const libusbp::error & error)
    {
      if (error.has_code(LIBUSBP_ERROR_NOT_READY))
      {
        // This interface is not ready to be used yet.
        // This is normal if it was recently enumerated.
        continue;
      }
      throw;
    }

    bootloader_instance instance(*type, usb_interface, device.get_serial_number());
    list.push_back(instance);
  }

  return list;
}

bootloader_handle::bootloader_handle(bootloader_instance instance)
  : type(instance.type)
{
  handle = libusbp::generic_handle(instance.usb_interface);
}

void bootloader_handle::initialize(uint16_t upload_type)
{
  try
  {
    handle.control_transfer(0x40, REQUEST_INITIALIZE, upload_type, 0);
  }
  catch(const libusbp::error & error)
  {
    throw std::runtime_error(
      std::string("Failed to initialize bootloader: ") +
      error.message());
  }
}

static std::runtime_error transfer_length_error(std::string context,
  size_t expected, size_t actual)
{
  return std::runtime_error(
    std::string("Incorrect transfer length while ") + context + ": "
    "expected " + std::to_string(expected) + ", "
    "got " + std::to_string(actual) + ".");
}

void bootloader_handle::erase_flash()
{
  int max_progress = 0;

  while (true)
  {
    uint8_t response[2];
    size_t transferred;
    handle.control_transfer(0xC0, REQUEST_ERASE_FLASH, 0, 0,
      &response, sizeof(response), &transferred);
    if (transferred != 2)
    {
      throw transfer_length_error("erasing flash", 2, transferred);
    }
    uint8_t error_code = response[0];
    uint8_t progress_left = response[1];
    if (error_code)
    {
      throw std::runtime_error("Error erasing page: " +
        bootloader_get_error_description(error_code) + ".");
    }

    if (max_progress < progress_left)
    {
      max_progress = progress_left + 1;
    }

    if (listener)
    {
      uint32_t progress = max_progress - progress_left;
      listener->set_status("Erasing flash...", progress, max_progress);
    }

    if (progress_left == 0)
    {
      return;
    }
  }
}

void bootloader_handle::restart_device()
{
  const uint16_t duration_ms = 100;
  try
  {
    handle.control_transfer(0x40, REQUEST_RESTART, duration_ms, 0);
  }
  catch(const libusbp::error & error)
  {
    throw std::runtime_error(
      std::string("Failed to restart device.") + error.what());
  }
}

void bootloader_handle::apply_image(const firmware_archive::image & image)
{
  initialize(image.upload_type);

  erase_flash();

  // We erase the first byte of EEPROM so that the firmware is able to
  // know it has been upgraded and not accidentally use invalid settings
  // from an older version of the firmware.
  erase_eeprom_first_byte();

  size_t progress = 0;
  for (const firmware_archive::block & block : image.blocks)
  {
    write_flash_block(block.address, &block.data[0], block.data.size());

    if (listener)
    {
      progress++;
      listener->set_status("Writing flash...", progress, image.blocks.size());
    }
  }
}

void bootloader_handle::write_flash_block(uint32_t address,
  const uint8_t * data, size_t size)
{
  size_t transferred;
  try
  {
    handle.control_transfer(0x40, REQUEST_WRITE_FLASH_BLOCK,
      address & 0xFFFF, address >> 16 & 0xFFFF,
      (uint8_t *)data, type.write_block_size, &transferred);
  }
  catch(const libusbp::error & error)
  {
    report_error(error, "Failed to write flash");
  }

  if (transferred != size)
  {
    throw transfer_length_error("writing flash",
      type.write_block_size, transferred);
  }
}

void bootloader_handle::write_eeprom_block(uint32_t address,
  const uint8_t * data, size_t size)
{
  size_t transferred;
  try
  {
    handle.control_transfer(0x40, REQUEST_WRITE_EEPROM,
      address & 0xFFFF, address >> 16 & 0xFFFF,
      (uint8_t *)data, size, &transferred);
  }
  catch(const libusbp::error & error)
  {
    report_error(error, "Failed to write EEPROM");
  }
  if (transferred != size)
  {
    throw transfer_length_error("writing EEPROM", size, transferred);
  }
}

void bootloader_handle::erase_eeprom_first_byte()
{
  uint8_t blank_byte = 0xFF;
  write_eeprom_block(0, &blank_byte, 1);
}

// This can be called after a USB request for writing EEPROM or flash fails.  If
// appropriate, it attempts to make another request to get a more specific error
// code from the device, and then throws an error with that information in it.
// If anything goes wrong, it just throws the original USB error.
void bootloader_handle::report_error(const libusbp::error & error,
  const std::string & context)
{
  if (!error.has_code(LIBUSBP_ERROR_STALL))
  {
    // This is an unusual error that was not just caused by a STALL packet,
    // so don't attempt to do anything.  Maybe the device didn't even see
    // the original USB request so the value returned by REQUEST_GET_LAST_ERROR
    // would be meaningless.
    throw error;
  }

  uint8_t error_code = 0;
  size_t transferred = 0;
  try
  {
    handle.control_transfer(0xC0, REQUEST_GET_LAST_ERROR, 0, 0,
      &error_code, 1, &transferred);
  }
  catch(const libusbp::error & second_error)
  {
    throw error;
  }

  if (transferred != 1)
  {
    throw error;
  }

  std::string message = context + ": " + bootloader_get_error_description(error_code);
  throw std::runtime_error(message);
}
