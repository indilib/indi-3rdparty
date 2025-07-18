#pragma once

#include <tic.hpp>
#include <file_util.h>
#include <string_to_int.h>
#include "config.h"

#include "arg_reader.h"
#include "device_selector.h"
#include "exit_codes.h"
#include "exception_with_exit_code.h"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

void print_status(const tic::variables & vars,
  const tic::settings & settings,
  const std::string & name,
  const std::string & serial_number,
  const std::string & firmware_version,
  bool full_output);
