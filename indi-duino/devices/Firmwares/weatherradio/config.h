/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019-20 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
*/

// BAUD rate for the serial interface
#define BAUD_RATE 9600   // standard rate that should always work
// #define BAUD_RATE 115200 // ESP8266

#define USE_BME_SENSOR            // USE BME280 ENVIRONMENT SENSOR.
//#define USE_DHT_SENSOR            // USE DHT HUMITITY SENSOR.
                                  // HINT: Edit dht.h for sensor specifics
#define USE_MLX_SENSOR            // USE MELEXIS 90614 IR SENSOR.
#define USE_TSL2591_SENSOR        // USE TSL2591 SENSOR.
//#define USE_TSL237_SENSOR         // USE TSL237 SENSOR.
//#define USE_DAVIS_SENSOR          // USE the Davis Anemometer.
//#define USE_WIFI                  // Run a web server on the Arduino (e.g. ESP8266 etc.)

// ============== device configurations (begin) ============

// DHT sensor family
#define DHTPIN 3          // Digital pin connected to the DHT sensor
//#define DHTTYPE DHT11   // DHT 11               - Uncomment whatever type you're using!
#define DHTTYPE DHT22     // DHT 22  (AM2302), AM2321 - Uncomment whatever type you're using!
//#define DHTTYPE DHT21   // DHT 21 (AM2301)      - Uncomment whatever type you're using!

// WiFi settings
#define WIFI_SSID "your WiFi SSID"
#define WIFI_PWD  "your WiFi password"

// Davis Anemometer
#define ANEMOMETER_WINDSPEEDPIN (2)    // The digital pin for the wind speed sensor
#define ANEMOMETER_WINDDIRECTIONPIN A0 // The analog pin for the wind direction
#define ANEMOMETER_WINDOFFSET 0        // anemometer arm direction (0=N, 90=E, ...)

// ============== device configurations (end) ==============


#ifdef USE_TSL2591_SENSOR
#include "tsl2591.h"
#endif //USE_TSL2591_SENSOR

#ifdef USE_TSL237_SENSOR
#include "tsl237.h"
#endif //USE_TSL237_SENSOR

#ifdef USE_BME_SENSOR
#include "bme280.h"
#endif //USE_BME_SENSOR

#ifdef USE_DHT_SENSOR
#include "dht.h"
#endif //USE_DHT_SENSOR

#ifdef USE_MLX_SENSOR
#include "mlx90614.h"
#endif //USE_MLX_SENSOR

#ifdef USE_DAVIS_SENSOR
#include "davis_anemometer.h"
#endif //USE_DAVIS_SENSOR

#ifdef USE_WIFI
#include "esp8266.h"
#endif
