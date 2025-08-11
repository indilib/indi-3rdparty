#include "to_string.h"
#include "tic.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

std::string convert_up_time_to_hms_string(uint32_t up_time)
{
  std::ostringstream ss;
  uint32_t seconds = up_time / 1000;
  uint32_t minutes = seconds / 60;
  uint16_t hours = minutes / 60;

  ss << hours <<
    ":" << std::setfill('0') << std::setw(2) << minutes % 60 <<
    ":" << std::setfill('0') << std::setw(2) << seconds % 60;
  return ss.str();
}

std::string convert_input_to_us_string(uint16_t input)
{
  std::ostringstream ss;
  uint16_t us = ((uint32_t)input * 4 + 3) / 6; // scale and round from units of 2/3 us

  ss << us << u8" \u03BCs";
  return ss.str();
}

std::string convert_input_to_v_string(uint16_t input)
{
  std::ostringstream ss;
  uint16_t mv = ((uint32_t)input * 4800 + 2048) / 4096; // 4096 = 4.8 V

  ss << (mv / 1000) << "." <<
    std::setfill('0') << std::setw(3) << (mv % 1000) << " V";
  return ss.str();
}

std::string convert_mv_to_v_string(uint32_t mv)
{
  std::ostringstream ss;
  uint32_t dv = (mv + 50) / 100;

  ss << (dv / 10) << "." << (dv % 10) << " V";
  return ss.str();
}

std::string convert_speed_to_pps_string(int32_t speed)
{
  static uint8_t const decimal_digits = std::log10(TIC_SPEED_UNITS_PER_HZ);

  std::ostringstream ss;
  std::string sign = (speed < 0) ? "-" : "";

  ss << sign << std::abs(speed / TIC_SPEED_UNITS_PER_HZ) << "." <<
    std::setfill('0') << std::setw(decimal_digits) <<
    std::abs(speed % TIC_SPEED_UNITS_PER_HZ) << " pulses/s";
  return ss.str();
}

std::string convert_accel_to_pps2_string(int32_t accel)
{
  static uint8_t const decimal_digits = std::log10(TIC_ACCEL_UNITS_PER_HZ2);

  std::ostringstream ss;
  std::string sign = (accel < 0) ? "-" : "";

  ss << sign << std::abs(accel / TIC_ACCEL_UNITS_PER_HZ2) << "." <<
    std::setfill('0') << std::setw(decimal_digits) <<
    std::abs(accel % TIC_ACCEL_UNITS_PER_HZ2) << u8" pulses/s\u00B2";
  return ss.str();
}