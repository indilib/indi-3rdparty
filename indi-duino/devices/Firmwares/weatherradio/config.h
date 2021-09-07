/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019-20 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
*/

#include "version.h"

// BAUD rate for the serial interface
#define BAUD_RATE 9600   // standard rate that should always work
//#define BAUD_RATE 115200 // modern boards like ESP8266

//#define USE_BME_SENSOR            // USE BME280 ENVIRONMENT SENSOR.
//#define USE_DHT_SENSOR            // USE DHT HUMITITY SENSOR.
                                  // HINT: Edit dht.h for sensor specifics
//#define USE_MLX_SENSOR            // USE MELEXIS 90614 IR SENSOR.
//#define USE_TSL2591_SENSOR        // USE TSL2591 SENSOR.
//#define USE_TSL237_SENSOR         // USE TSL237 SENSOR.
//#define USE_DAVIS_SENSOR          // USE the Davis Anemometer.
//#define USE_WATER_SENSOR        // Water sensor - detects rain by measuring resistence
//#define USE_RG11_RAIN_SENSOR      // USE the RG-11 rain sensor
//#define USE_W174_RAIN_SENSOR      // USE the Ventus W174 rain sensor
//#define USE_WIFI                  // Run a web server on the Arduino (e.g. ESP8266 etc.)
//#define USE_OLED                  // USE a OLED display
//#define USE_OTA                   // USE Arduino Over the Air updating
//#define USE_DEWHEATER             // Resistor based dew heater

// refresh cache interval (ms)
#define MAX_CACHE_AGE 30000

// verbosity level
#define MESSAGE_VERBOSITY MESSAGE_INFO
// maximal size of buffered JSON messages
#define MAX_JSON_BUFFER_SIZE 16000

// ============== device configurations (begin) ============

// DHT sensor family
#define DHTPIN 13          // Digital pin connected to the DHT sensor
//#define DHTTYPE DHT11   // DHT 11               - Uncomment whatever type you're using!
#define DHTTYPE DHT22     // DHT 22  (AM2302), AM2321 - Uncomment whatever type you're using!
//#define DHTTYPE DHT21   // DHT 21 (AM2301)      - Uncomment whatever type you're using!

// WiFi settings
// Fill in your WiFi ID and password - leave empty if nothing has changed
#define WIFI_SSID ""
#define WIFI_PWD  ""

// Davis Anemometer
#define ANEMOMETER_WINDSPEEDPIN (2)    // The digital pin for the wind speed sensor
#define ANEMOMETER_WINDDIRECTIONPIN A0 // The analog pin for the wind direction
#define ANEMOMETER_WINDOFFSET 0        // anemometer arm direction (0=N, 90=E, ...)

// Resistor based water sensor
#define WATER_PIN A0

// rain sensor for RG-11
#define RG11_RAINSENSOR_PIN 13            // the digital pin for the rain sensor
#define RG11_MODE 1                       // Use either 1 for "Drop Detector" or 0 for "Tipping Bucket"
#define RG11_RAINSENSOR_BUCKET_SIZE 0.001 // the bucket size in mm configured with the dip switches

// dipping bucket rain sensor for Ventus W174
#define W174_RAINSENSOR_PIN 12            // the digital pin for the rain sensor
#define W174_RAINSENSOR_BUCKET_SIZE 0.5   // the bucket size: 28.46cm², 1.42ml bucket size --> 1x bucket full = 0.5mm rain fall

// Resistor based dew heater
#define DEWHEATER_MAX_DELTA 1 // maximum allowable temperature difference between ambient and dew point before heater turns on
#define DEWHEATER_MIN_TEMPERATURE 0 // Minimum temperature below which dew heater turns on to combat snow and ice
#define DEWHEATER_PIN 5

// OLED display
#define OLED_SCROLL_TIMEOUT 100       // the timeout between scrolling a single display line
#define OLED_DISPLAY_TIMEOUT 60       // the timeout in secs after which the display will turn off (set to -1 to disable the timeout)
#define OLED_BUTTONPIN 15             // pin for button to turn display on
#define OLED_I2C_ADDRESS    0x3C      // I2C address of the OLED display - consult data sheet
#define OLED_WIRE_CLOCK_SPEED 100000L // set to 100kHz if using in combination with MLX90614

// ============== device configurations (end) ==============

#include "jsonmessage.h"

#ifdef USE_OLED
#include "oled.h"
#endif // USE_OLED

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

#ifdef USE_WATER_SENSOR
#include "water.h"
#endif //USE_WATER_SENSOR

#ifdef USE_DEWHEATER
#include "dewheater.h"
#endif //USE_DEWHEATER

#ifdef USE_RG11_RAIN_SENSOR
#include "rg11.h"
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
#include "w174.h"
#endif //USE_W174_RAIN_SENSOR

#ifdef USE_WIFI
#include "esp8266.h"
#endif

#ifdef USE_OTA
#include "ota.h"
#endif // USE_OTA
