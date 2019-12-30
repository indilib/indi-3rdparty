/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include <ArduinoJson.h>
// #include <MemoryFree.h> // only necessary for debugging


#define USE_BME_SENSOR            //USE BME280 ENVIRONMENT SENSOR. Comment if not.
#define USE_MLX_SENSOR            //USE MELEXIS IR SENSOR. Comment if not.
#define USE_TSL_SENSOR            //USE TSL2591 SENSOR. Comment if not.

#ifdef USE_TSL_SENSOR
#include "tsl2591.h"
#endif //USE_TSL_SENSOR

#ifdef USE_BME_SENSOR
#include "bme280.h"
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
#include "mlx90614.h"
#endif //USE_MLX_SENSOR

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

}

void loop() {

  StaticJsonDocument < JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(1) > weatherDoc;

#ifdef USE_BME_SENSOR
  updateBME();
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
  updateMLX();
  serializeMLX(weatherDoc);
#endif //USE_MLX_SENSOR

#ifdef USE_TSL_SENSOR
  updateTSL();
  serializeTSL(weatherDoc);
#endif //USE_TSL_SENSOR

  serializeJson(weatherDoc, Serial);
  Serial.println();

  delay(1000);
  //Serial.print("free Memory="); Serial.println(freeMemory());
}
