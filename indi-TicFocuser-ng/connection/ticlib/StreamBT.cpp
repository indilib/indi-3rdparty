/*******************************************************************************
TicFocuser
Copyright (C) 2019 Sebastian BaberowbtSocketi

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

#include "StreamBT.h"

#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

StreamBT::StreamBT():
	btSocket(-1)
{
}

StreamBT::~StreamBT()
{
	disconnect();
}

bool StreamBT::reconnect()
{
  if (btMacAddress.empty())
    return false;

  struct sockaddr_rc laddr, raddr;

  laddr.rc_family = AF_BLUETOOTH;
  laddr.rc_channel = 0;
  str2ba("00:00:00:00:00:00",&laddr.rc_bdaddr);
  //bacpy(&laddr.rc_bdaddr,BDADDR_ANY);

  raddr.rc_family = AF_BLUETOOTH;
  raddr.rc_channel = 1;
  str2ba(btMacAddress.c_str(),&raddr.rc_bdaddr);

  btSocket = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  if (btSocket < 0)
    return false;

  timeval timeout = { 1, 0 };
  if (setsockopt(btSocket, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout))) {
    close(btSocket);
    return false;    
  }

  if (setsockopt(btSocket, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout))) {
    close(btSocket);
    return false;    
  }

  if (::bind(btSocket, (struct sockaddr *) &laddr, sizeof(laddr)) < 0) {
    close(btSocket);
    return false;
  }

  if (::connect(btSocket, (struct sockaddr *) &raddr, sizeof(raddr)) < 0) {
    close(btSocket);
    return false;
  }

  return true;  
}

bool StreamBT::connect(const char* btMacAddress)
{
  StreamBT::btMacAddress = btMacAddress;
  return reconnect();
}

void StreamBT::disconnect()
{
	if (btSocket >= 0)
		close(btSocket);
	btSocket = -1;
}

size_t StreamBT::write(uint8_t byte)
{
  ssize_t c = 0;
  int numZeros = 0; // safety counter, if we read 5x0 in a row, we eject

  while (numZeros < 3)
  {
    c = ::write(btSocket,&byte,sizeof(byte));

    if (c > 0)
      break;

    reconnect();
    usleep(10);
    ++numZeros;
  }

  return c;
}

size_t StreamBT::readBytes(char *buffer, size_t length)
{
  size_t readC = 0;
  int numZeros = 0; // safety counter, if we read 5x0 in a row, we eject

  // try to read until we receive enough bytes
  while (readC < length && numZeros < 5)
  {

    if (readC > 0)
      usleep(10);
    
    ssize_t c = ::read(btSocket, buffer + readC, length - readC);

    // BT got disconnected
    if (c < 0) {
      reconnect();
    }

    if (c > 0)
    {
      readC += c;
      numZeros = 0;
    }
    else
    {
      ++numZeros;
    }
  }

  return readC;
}

