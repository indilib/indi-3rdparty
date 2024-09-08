#pragma once

#include <libusb.h>

#define MAXRBUF 2048

int fx2_ram_download (libusb_device_handle *h, char *filename, unsigned char extended, char *errmsg);
