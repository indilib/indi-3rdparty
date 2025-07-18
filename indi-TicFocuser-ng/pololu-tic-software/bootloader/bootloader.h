#pragma once

#include "firmware_archive.h"
#include <libusbp.hpp>
#include <vector>

typedef std::vector<uint8_t> memory_image;

#define UPLOAD_TYPE_STANDARD 0
#define UPLOAD_TYPE_DEVICE_SPECIFIC 1
#define UPLOAD_TYPE_PLAIN 2

// Represents a type of bootloader.
class bootloader_type
{
public:
  uint32_t id;
  uint16_t usb_vendor_id, usb_product_id;

  // A pointer to an ASCII string with the name of the bootloader.
  // Should be the same as the USB product string descriptor.
  const char * name;

  const char * short_name;

  // The address of the first byte of the app (used in the USB protocol).
  uint32_t app_address;

  // The number of bytes in the app.
  uint32_t app_size;

  // The number of bytes you have to write at once while writing to flash.
  uint16_t write_block_size;

  // The address of the first byte of EEPROM (used in the USB protocol).
  uint32_t eeprom_address;

  // The address used for the first byte of EEPROM in the HEX file.
  uint32_t eeprom_address_hex_file;

  // The number of bytes in EEPROM.  Set this to a non-zero number for devices
  // that have EEPROM, even if the bootloader does not support accessing
  // EEPROM.
  uint32_t eeprom_size;

  bool operator ==(const bootloader_type & other) const
  {
    return id == other.id;
  }
};

extern const std::vector<bootloader_type> bootloader_types;

const bootloader_type * bootloader_type_lookup(
  uint16_t usb_vendor_id, uint16_t usb_product_id);

// Represents a specific bootloader connected to the system
// and ready to be used.
class bootloader_instance
{
public:
  bootloader_type type;
  std::string serial_number;

  bootloader_instance()
  {
  }

  bootloader_instance(const bootloader_type type,
    libusbp::generic_interface gi,
    std::string serial_number)
    : type(type), serial_number(serial_number), usb_interface(gi)
  {
  }

  operator bool()
  {
    return usb_interface;
  }

  std::string get_short_name() const
  {
    return type.short_name;
  }

  std::string get_serial_number() const
  {
    return serial_number;
  }

  std::string get_os_id() const
  {
    return usb_interface.get_os_id();
  }

  uint16_t get_vendor_id() const
  {
    return type.usb_vendor_id;
  }

  uint16_t get_product_id() const
  {
    return type.usb_product_id;
  }

  libusbp::generic_interface usb_interface;
};

// Detects all the known bootloaders that are currently connected to the
// computer.
std::vector<bootloader_instance> bootloader_list_connected_devices();

class bootloder_status_listener
{
public:
  virtual void set_status(const char * status,
    uint32_t progress, uint32_t max_progress) = 0;
};

class bootloader_handle
{
public:
  bootloader_handle(bootloader_instance);

  bootloader_handle() { }

  operator bool() const noexcept { return handle; }

  void close()
  {
    *this = bootloader_handle();
  }

  // Sends a request to the bootloader to initialize a particular type of
  // upload.  This is usually required before erasing or writing to flash.
  void initialize(uint16_t upload_type);

  // Erases the entire application flash region.
  void erase_flash();

  // Sends the Restart command, which causes the device to reset.  This is
  // usually used to allow a newly-loaded application to start running.
  void restart_device();

  // Erases flash and performs any other steps needed to apply the firmware
  // image to the device
  void apply_image(const firmware_archive::image & image);

  void set_status_listener(bootloder_status_listener * listener)
  {
    this->listener = listener;
  }

  bootloader_type type;

private:
  void write_flash_block(const uint32_t address, const uint8_t * data, size_t size);
  void write_eeprom_block(const uint32_t address, const uint8_t * data, size_t size);
  void erase_eeprom_first_byte();

  void report_error(const libusbp::error & error, const std::string & context)
    __attribute__((noreturn));

  bootloder_status_listener * listener;

  libusbp::generic_handle handle;
};
