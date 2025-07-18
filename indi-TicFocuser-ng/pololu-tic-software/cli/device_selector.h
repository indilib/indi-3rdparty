#pragma once

#include "exit_codes.h"
#include "exception_with_exit_code.h"
#include <cassert>

// Contains the logic for finding devices and choosing which one to use.
class device_selector
{
public:
  void specify_serial_number(const std::string & serial_number)
  {
    assert(!list_initialized);
    this->serial_number = serial_number;
    this->serial_number_specified = true;
  }

  std::vector<tic::device> list_devices()
  {
    if (list_initialized) { return list; }

    list.clear();
    for (tic::device & device : tic::list_connected_devices())
    {
      if (serial_number_specified && device.get_serial_number() != serial_number)
      {
        continue;
      }

      list.push_back(std::move(device));
    }
    list_initialized = true;
    return list;
  }

  tic::device select_device()
  {
    if (device) { return device; }

    auto list = list_devices();
    if (list.size() == 0)
    {
      throw device_not_found_error();
    }

    if (list.size() > 1)
    {
      throw device_multiple_found_error();
    }

    device = list[0];
    return device;
  }

private:

  std::string device_not_found_message() const
  {
    std::string r = "No device was found";

    if (serial_number_specified)
    {
      r += std::string(" with serial number '") + serial_number + "'";
    }

    r += ".";
    return r;
  }

  exception_with_exit_code device_not_found_error() const
  {
      return exception_with_exit_code(
        EXIT_DEVICE_NOT_FOUND, device_not_found_message());
  }

  exception_with_exit_code device_multiple_found_error() const
  {
    return exception_with_exit_code(
      EXIT_DEVICE_MULTIPLE_FOUND,
      "There are multiple qualifying devices connected to this computer.\n"
      "Use the -d option to specify which device you want to use,\n"
      "or disconnect the others.");
  }

  bool serial_number_specified = false;
  std::string serial_number;

  bool list_initialized = false;
  std::vector<tic::device> list;

  tic::device device;
};
