#pragma once

#include <cstdint>
#include <string>

std::string convert_up_time_to_hms_string(uint32_t up_time);
std::string convert_input_to_us_string(uint16_t input);
std::string convert_input_to_v_string(uint16_t input);
std::string convert_mv_to_v_string(uint32_t mv);
std::string convert_speed_to_pps_string(int32_t speed);
std::string convert_accel_to_pps2_string(int32_t accel);