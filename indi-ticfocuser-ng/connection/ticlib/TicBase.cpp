// Copyright (C) Pololu Corporation.  See LICENSE.txt for details.

#include "TicBase.h"
#include <string.h>

static const uint16_t Tic03aCurrentTable[33] =
{
  0,
  1,
  174,
  343,
  495,
  634,
  762,
  880,
  990,
  1092,
  1189,
  1281,
  1368,
  1452,
  1532,
  1611,
  1687,
  1762,
  1835,
  1909,
  1982,
  2056,
  2131,
  2207,
  2285,
  2366,
  2451,
  2540,
  2634,
  2734,
  2843,
  2962,
  3093,
};

void TicBase::setCurrentLimit(uint16_t limit)
{
  uint8_t code = 0;

  if (product == TicProduct::T500)
  {
    for (uint8_t i = 0; i < 33; i++)
    {
      if (Tic03aCurrentTable[i] <= limit)
      {
        code = i;
      }
      else
      {
        break;
      }
    }
  }
  else if (product == TicProduct::T249)
  {
    code = limit / TicT249CurrentUnits;
  }
  else if (product == TicProduct::Tic36v4)
  {
    if (limit < 72) { code = 0; }
    else if (limit >= 9095) { code = 127; }
    else
    {
      code = ((uint32_t)limit * 768 - 55000 / 2) / 55000;
      if (code < 127 && (((uint32_t)55000 * (code + 1) + 384) / 768) <= limit)
      {
        code++;
      }
    }
  }
  else
  {
    code = limit / TicCurrentUnits;
  }

  commandW7(TicCommand::SetCurrentLimit, code);
}

uint16_t TicBase::getCurrentLimit()
{
  uint8_t code = getVar8(VarOffset::CurrentLimit);
  if (product == TicProduct::T500)
  {
    if (code > 32) { code = 32; }
    return Tic03aCurrentTable[code];
  }
  else if (product == TicProduct::T249)
  {
    return code * TicT249CurrentUnits;
  }
  else if (product == TicProduct::Tic36v4)
  {
    return ((uint32_t)55000 * code + 384) / 768;
  }
  else
  {
    return code * TicCurrentUnits;
  }
}

/**** TicSerial ****/

void TicSerial::commandW32(TicCommand cmd, uint32_t val)
{
  sendCommandHeader(cmd);

  // byte with MSbs:
  // bit 0 = MSb of first (least significant) data byte
  // bit 1 = MSb of second data byte
  // bit 2 = MSb of third data byte
  // bit 3 = MSb of fourth (most significant) data byte
  serialW7(((val >>  7) & 1) |
           ((val >> 14) & 2) |
           ((val >> 21) & 4) |
           ((val >> 28) & 8));

  serialW7(val >> 0); // least significant byte with MSb cleared
  serialW7(val >> 8);
  serialW7(val >> 16);
  serialW7(val >> 24); // most significant byte with MSb cleared

  _lastError = 0;
}

void TicSerial::commandW7(TicCommand cmd, uint8_t val)
{
  sendCommandHeader(cmd);
  serialW7(val);

  _lastError = 0;
}

void TicSerial::getSegment(TicCommand cmd, uint8_t offset,
  uint8_t length, void * buffer)
{
  length &= 0x3F;
  sendCommandHeader(cmd);
  serialW7(offset & 0x7F);
  serialW7(length | (offset >> 1 & 0x40));

  uint8_t byteCount = _stream->readBytes((uint8_t *)buffer, length);
  if (byteCount != length)
  {
    _lastError = 50;

    // Set the buffer bytes to 0 so the program will not use an uninitialized
    // variable.
    memset(buffer, 0, length);
    return;
  }

  _lastError = 0;
}

void TicSerial::sendCommandHeader(TicCommand cmd)
{
  if (_deviceNumber == 255)
  {
    // Compact protocol
    _stream->write((uint8_t)cmd);
  }
  else
  {
    // Pololu protocol
    _stream->write((uint8_t)0xAA);
    serialW7(_deviceNumber);
    serialW7((uint8_t)cmd);
  }
  _lastError = 0;
}

