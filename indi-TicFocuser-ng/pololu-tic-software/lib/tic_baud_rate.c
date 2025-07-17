#include "tic_internal.h"

// Converts the baud rate generator value as stored on the EEPROM to the actual
// baud rate in bits per second it represents.
uint32_t tic_baud_rate_from_brg(uint16_t brg)
{
  return (TIC_BAUD_RATE_GENERATOR_FACTOR + ((brg + 1) >> 1)) / (brg + 1);
}

// Converts a baud rate in bits per second to the nearest baud rate generator
// value.
uint16_t tic_baud_rate_to_brg(uint32_t baud_rate)
{
  if (baud_rate == 0)
  {
    return 0xFFFF;
  }

  uint32_t brg = (TIC_BAUD_RATE_GENERATOR_FACTOR - (baud_rate >> 1)) / baud_rate;

  if (brg > 0xFFFF)
  {
    return 0xFFFF;
  }

  return brg;
}

