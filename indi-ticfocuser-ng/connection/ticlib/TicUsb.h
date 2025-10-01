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

#ifndef TICUSB_H
#define TICUSB_H

#include "TicBase.h"
#include <string>

struct libusb_device_handle;
struct libusb_context;

class TicUsb: public TicBase {

	libusb_device_handle *handle;
	libusb_context *context;
	std::string serialNumber;

public:

	TicUsb();
	~TicUsb();

	void connect(const char* serialNo);
	void disconnect();
	const char* getSerial()	{ return serialNumber.c_str(); }

	void commandQuick(TicCommand cmd);
	void commandW32(TicCommand cmd, uint32_t val);
	void commandW7(TicCommand cmd, uint8_t val);
	void getSegment(TicCommand cmd, uint8_t offset, uint8_t length, void * buffer);

	const char* getLastErrorMsg();
};

#endif // TICUSB_H