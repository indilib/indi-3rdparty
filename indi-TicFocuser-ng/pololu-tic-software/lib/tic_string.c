// A library that lets us easily and safely work with null-terminated strings.

#include "tic_internal.h"

void tic_string_free(char * data)
{
  free(data);
}

void tic_string_setup(tic_string * str)
{
  assert(str != NULL);
  str->data = malloc(1);
  if (str->data != NULL)
  {
    str->capacity = 1;
    str->data[0] = 0;
  }
  else
  {
    str->capacity = 0;
  }
  str->length = 0;
}

// Sets up a dummy string that won't actually hold anything.
void tic_string_setup_dummy(tic_string * str)
{
  assert(str != NULL);
  str->data = NULL;
  str->capacity = str->length = 0;
}

void tic_sprintf(tic_string * str, const char * format, ...)
{
  assert(format != NULL);

  if (str->data == NULL)
  {
    // This is a dummy string.
    return;
  }

  assert(str->length == strlen(str->data));

  va_list ap;
  va_start(ap, format);

  // Determine how much the string length will increase.
  size_t length_increase = 0;
  {
    char x[1];
    va_list ap2;
    va_copy(ap2, ap);
    int result = vsnprintf(x, 0, format, ap2);
    va_end(ap2);
    if (result > 0)
    {
      length_increase = result;
    }
    else
    {
      // This error seems really unlikely to happen.  If it does, we can add a
      // better way to report it.  For now, just turn the string into a dummy
      // string.
      free(str->data);
      str->data = NULL;
      str->length = str->capacity = 0;
      va_end(ap);
      return;
    }
  }
  size_t new_length = str->length + length_increase;
  if (new_length + 1 < str->length)
  {
    // The capacity required to store this string (new_length + 1) has
    // overflowed and is too large to fit in a size_t.  Turn it into a dummy
    // string.
    free(str->data);
    str->data = NULL;
    str->length = str->capacity = 0;
    va_end(ap);
    return;
  }

  if (new_length + 1 > str->capacity)
  {
    // Need to reallocate memory to fit the expanded string.

    // Figure out what the new capacity should be, but watch out for integer
    // overflow.
    size_t new_capacity = (new_length + 1) * 2;
    if (new_capacity < new_length + 1)
    {
      new_capacity = new_length + 1;
    }

    char * resized_data = realloc(str->data, new_capacity);
    if (resized_data == NULL)
    {
      // Failed to allocate memory, so let this just be a dummy string.
      free(str->data);
      str->data = NULL;
      str->length = str->capacity = 0;
      va_end(ap);
      return;
    }
    str->data = resized_data;
    str->capacity = new_capacity;
  }

  // Add the new content and a null terminator.
  int result = vsnprintf(str->data + str->length, length_increase + 1, format, ap);
  (void)result;  // suppress unused variable warnings in release builds
  assert((size_t)result == length_increase);
  str->data[new_length] = 0;
  str->length = new_length;

  assert(str->length == strlen(str->data));
  va_end(ap);
}

// This is derived from string_to_int.h
uint8_t tic_string_to_i64(const char * str, int64_t * out)
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

  int64_t result = 0;
  while (*p)
  {
    bool is_digit = *p >= '0' && *p <= '9';
    if (!is_digit)
    {
      return STRING_TO_INT_ERR_INVALID;  // Expected a digit, got something else.
    }

    uint8_t digit_value = *p - '0';

    if (negative)
    {
      if (result < (-0x7FFFFFFFFFFFFFFF - 1) / 10)
      {
        return STRING_TO_INT_ERR_SMALL;  // Multiplication would underflow.
      }
      result *= 10;

      if (result < (-0x7FFFFFFFFFFFFFFF - 1) + digit_value)
      {
        return STRING_TO_INT_ERR_SMALL;  // Subtraction would underflow.
      }
      result -= digit_value;
    }
    else
    {
      if (result > 0x7FFFFFFFFFFFFFFF / 10)
      {
        return STRING_TO_INT_ERR_LARGE;  // Multiplication would overflow.
      }
      result *= 10;

      if (result > 0x7FFFFFFFFFFFFFFF - digit_value)
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
