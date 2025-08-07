/*******************************************************************************
TicFocuser
Copyright (C) 2019 Sebastian Baberowski

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*******************************************************************************/

#include "TicUsb.h"
#include "TicDefs.h"

#include <libusb.h>
#include <string.h>

static const size_t MAX_SERIAL_NUMBER = 20; // serial number has 8 characters, 20 is super safe
int _lastError;

TicUsb::TicUsb():
  handle(NULL), context(NULL)
{
  _lastError = libusb_init(&context);
}

TicUsb::~TicUsb() 
{
  disconnect();
  
  if (context)
    libusb_exit(context);

  context = NULL;
}

void TicUsb::connect(const char* serialNo)
{
  if (!context)
    return;

  disconnect();

  libusb_device** devs = NULL;
  _lastError = libusb_get_device_list(context, &devs);
  if (_lastError <= 0)
    return;
  
  _lastError = 0;

  size_t serialNoLen = serialNo? strlen(serialNo): 0;
  libusb_device** dev = devs;

  for (; *dev != NULL; ++dev) 
  {
    libusb_device_descriptor desc;
    _lastError = libusb_get_device_descriptor(*dev, &desc);
    if (_lastError)
      break;

    if (desc.idVendor != TIC_VENDOR_ID ||
        (desc.idProduct != TIC_PRODUCT_ID_T825 &&
         desc.idProduct != TIC_PRODUCT_ID_T834 &&
         desc.idProduct != TIC_PRODUCT_ID_T500 &&
         desc.idProduct != TIC_PRODUCT_ID_N825 &&
         desc.idProduct != TIC_PRODUCT_ID_T249 &&
         desc.idProduct != TIC_PRODUCT_ID_36V4) )
    {
      continue;
    }

    if (!desc.iSerialNumber)
      continue;

    _lastError = libusb_open(*dev, &handle);
    if (_lastError) 
    {
      handle = NULL;
      break;
    }

    unsigned char devSerial[MAX_SERIAL_NUMBER];
    size_t serialLen = 0;

    serialLen = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, devSerial, MAX_SERIAL_NUMBER);
    if (
        (serialNoLen == 0 || 
          (serialLen == serialNoLen && !strncmp(serialNo,(char*)devSerial,serialLen))) ) 
    {
      serialNumber = (const char*)devSerial;
      break;
    }

    libusb_close(handle);
    handle = NULL;      
    _lastError = serialLen > 0? 0: serialLen;
  }

  libusb_free_device_list(devs,1);

  if (_lastError)
    return;

  if (!handle)
    _lastError = LIBUSB_ERROR_NO_DEVICE;
}

void TicUsb::disconnect()
{
  if (handle)
    libusb_close(handle);
  
  handle = NULL;
  serialNumber.clear();
}

void TicUsb::commandQuick(TicCommand cmd)  
{
  _lastError = libusb_control_transfer(handle, 0x40, (uint8_t)cmd, 0, 0, NULL, 0, 0);
  _lastError = _lastError > 0? 0: _lastError;
}
void TicUsb::commandW32(TicCommand cmd, uint32_t val)  {

  uint16_t wValue = (uint32_t)val;
  uint16_t wIndex = (uint32_t)val >> 16;
  
  _lastError = libusb_control_transfer(handle, 0x40, (uint8_t)cmd, wValue, wIndex, NULL, 0, 0);
  _lastError = _lastError > 0? 0: _lastError;
}

void TicUsb::commandW7(TicCommand cmd, uint8_t val)  
{
  _lastError = libusb_control_transfer(handle, 0x40, (uint8_t)cmd, val, 0, NULL, 0, 0);
  _lastError = _lastError > 0? 0: _lastError;
}

void TicUsb::getSegment(TicCommand cmd, uint8_t offset, uint8_t length, void * buffer)  
{
  _lastError = libusb_control_transfer(handle, 0xC0, (uint8_t)cmd, 0, offset, (unsigned char*)buffer, length, 0);

  if (_lastError != length)
    _lastError = LIBUSB_ERROR_OTHER;
  else
    _lastError = 0;
}

const char* TicUsb::getLastErrorMsg()
{
  return libusb_error_name( _lastError);
}
