/*  Memory information for Arduino.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

// memory information for ESP8266
extern "C" {
#include "user_interface.h"
}

uint32_t freeMemory() {
  return system_get_free_heap_size();
}
