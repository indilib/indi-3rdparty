// Helpers for converting strings to integers.
//
// We don't use strtoll because there is some bug that causes it to to always
// return error 22 when run inside a Qt application on MacOS.

#pragma once

#include <stdint.h>
#include <type_traits>
#include <limits>

#define STRING_TO_INT_ERR_SMALL 1
#define STRING_TO_INT_ERR_LARGE 2
#define STRING_TO_INT_ERR_EMPTY 3
#define STRING_TO_INT_ERR_INVALID 4

// Converts a decimal string to the specified integer type, returning an error
// if there is non-number junk in the string or the number is out of range.
template <typename T>
static uint8_t string_to_int(const char * str, T * out)
{
  *out = 0;

  const char * p = str;

  // Process minus and plus signs.
  bool negative = false;
  if (*p == '-')
  {
    negative = true;
    p++;
  }
  if (*p == '+')
  {
    p++;
  }

  // Reject numbers with no digits.
  if (*p == 0)
  {
    return STRING_TO_INT_ERR_EMPTY;
  }

  T result = 0;
  while (*p)
  {
    uint8_t digit_value;
    if (*p >= '0' && *p <= '9')
    {
      digit_value = *p - '0';
    }
    else
    {
      // Expected a digit, got something else.
      return STRING_TO_INT_ERR_INVALID;
    }

    if (negative)
    {
      if (result < std::numeric_limits<T>::min() / 10)
      {
        return STRING_TO_INT_ERR_SMALL;  // Multiplication would underflow.
      }
      result *= 10;

      if (result < std::numeric_limits<T>::min() + digit_value)
      {
        return STRING_TO_INT_ERR_SMALL;  // Subtraction would underflow.
      }
      result -= digit_value;
    }
    else
    {
      if (result > std::numeric_limits<T>::max() / 10)
      {
        return STRING_TO_INT_ERR_LARGE;  // Multiplication would overflow.
      }
      result *= 10;

      if (result > std::numeric_limits<T>::max() - digit_value)
      {
        return STRING_TO_INT_ERR_LARGE;  // Addition would overflow.
      }
      result += digit_value;
    }

    p++;
  }

  *out = result;
  return 0;
}

// Converts an unprefixed hex string to the specified integer type, returning
// an error if there is non-number junk in the string or the number is out of
// range.
template <typename T>
static uint8_t hex_string_to_int(const char * str, T * out)
{
  *out = 0;

  const char * p = str;

  // Process minus and plus signs.
  bool negative = false;
  if (*p == '-')
  {
    negative = true;
    p++;
  }
  if (*p == '+')
  {
    p++;
  }

  // Reject numbers with no digits.
  if (*p == 0)
  {
    return STRING_TO_INT_ERR_EMPTY;
  }

  T result = 0;
  while (*p)
  {
    uint8_t digit_value;
    if (*p >= '0' && *p <= '9')
    {
      digit_value = *p - '0';
    }
    else if (*p >= 'a' && *p <= 'f')
    {
      digit_value = *p - 'a' + 10;
    }
    else if (*p >= 'A' && *p <= 'F')
    {
      digit_value = *p - 'A' + 10;
    }
    else
    {
      // Expected a digit, got something else.
      return STRING_TO_INT_ERR_INVALID;
    }

    if (negative)
    {
      if (result < std::numeric_limits<T>::min() / 0x10)
      {
        return STRING_TO_INT_ERR_SMALL;  // Multiplication would underflow.
      }
      result *= 0x10;

      if (result < std::numeric_limits<T>::min() + digit_value)
      {
        return STRING_TO_INT_ERR_SMALL;  // Subtraction would underflow.
      }
      result -= digit_value;
    }
    else
    {
      if (result > std::numeric_limits<T>::max() / 0x10)
      {
        return STRING_TO_INT_ERR_LARGE;  // Multiplication would overflow.
      }
      result *= 0x10;

      if (result > std::numeric_limits<T>::max() - digit_value)
      {
        return STRING_TO_INT_ERR_LARGE;  // Addition would overflow.
      }
      result += digit_value;
    }

    p++;
  }

  *out = result;
  return 0;
}

// Parse an integer that might be prefixed with "0x" or "0b".
// Advances the pointer to point past the end of the integer
// (so this does not detect junk in your string after the integer).
template <typename T>
static uint8_t parse_prefixed_int(const char * & p, T * out)
{
  *out = 0;

  // Process minus and plus signs.
  bool negative = false;
  if (*p == '-')
  {
    negative = true;
    p++;
  }
  if (*p == '+')
  {
    p++;
  }

  // Process prefixes.
  uint8_t base = 10;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
  {
    base = 16;
    p += 2;
  }
  if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B'))
  {
    base = 2;
    p += 2;
  }

  bool found_digit = false;
  T result = 0;
  while (true)
  {
    uint8_t digit_value;
    if (*p >= '0' && *p <= '9' && *p < '0' + base)
    {
      digit_value = *p - '0';
    }
    else if (base == 16 && *p >= 'a' && *p <= 'f')
    {
      digit_value = *p - 'a' + 10;
    }
    else if (base == 16 && *p >= 'A' && *p <= 'F')
    {
      digit_value = *p - 'A' + 10;
    }
    else
    {
      // We reached the end of the number.
      break;
    }

    // Process the digit we just found.
    found_digit = true;

    if (negative)
    {
      if (result < std::numeric_limits<T>::min() / base)
      {
        return STRING_TO_INT_ERR_SMALL;  // Multiplication would underflow.
      }
      result *= base;

      if (result < std::numeric_limits<T>::min() + digit_value)
      {
        return STRING_TO_INT_ERR_SMALL;  // Subtraction would underflow.
      }
      result -= digit_value;
    }
    else
    {
      if (result > std::numeric_limits<T>::max() / base)
      {
        return STRING_TO_INT_ERR_LARGE;  // Multiplication would overflow.
      }
      result *= base;

      if (result > std::numeric_limits<T>::max() - digit_value)
      {
        return STRING_TO_INT_ERR_LARGE;  // Addition would overflow.
      }
      result += digit_value;
    }

    p++;
  }

  if (!found_digit)
  {
    // We reached the end of the number before finding any digits.
    return STRING_TO_INT_ERR_EMPTY;
  }

  *out = result;
  return 0;
}
