#pragma once

#include <stdexcept>

class exception_with_exit_code : public std::exception
{
public:
  explicit exception_with_exit_code(uint8_t code, std::string message)
    : code(code), msg(message)
  {
  }

  virtual const char * what() const noexcept
  {
    return msg.c_str();
  }

  std::string message() const
  {
    return msg;
  }

  uint8_t get_code() const noexcept
  {
    return code;
  }

private:
  uint8_t code;
  std::string msg;
};
