/*  Firmware for PWM based dimmer - for flat fields and for dew bands.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "version.h"

// BAUD rate for the serial interface
// #define BAUD_RATE 9600   // standard rate that should always work
#define BAUD_RATE 115200 // modern boards like ESP8266

#define USE_WIFI                  // Run a web server on the Arduino (e.g. ESP8266 etc.)

// verbosity level
#define MESSAGE_VERBOSITY MESSAGE_INFO
// maximal size of buffered JSON messages
#define MAX_JSON_BUFFER_SIZE 16000

// ============== device configurations (begin) ============

// WiFi settings
// Fill in your WiFi ID and password - leave empty if nothing has changed
#define WIFI_SSID ""
#define WIFI_PWD  ""

// pin sending the PWM signal
#define PWM_PIN 13 // D7
// PWM default frequency in Hz
#define PWM_FREQ_DEFAULT 40000
// PWM default duty cycle (0..255)
#define PWM_DUTY_CYCLE_DEFAULT 128 


// ============== device configurations (end) ==============

#include "jsonmessage.h"

#ifdef USE_WIFI
#include "esp8266.h"
#endif
