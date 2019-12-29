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

#ifdef USE_BME_SENSOR
#include "Adafruit_BME280.h"
Adafruit_BME280 bme;
struct {
  bool status;
  float temperature;
  float pressure;
  float humidity;
} bmeData;


void serializeBME(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("BME280");
  data["T"] = bmeData.temperature;
  data["P"] = bmeData.pressure;
  data["H"] = bmeData.humidity;
}

void updateBME() {
  if (bmeData.status || (bmeData.status = bme.begin())) {
    bmeData.temperature = bme.readTemperature();
    bmeData.pressure    = bme.readPressure() / 100.0;
    bmeData.humidity    = bme.readHumidity();
  }
  else {
    bmeData.status = false;
    Serial.println("BME sensor initialization FAILED!");
  }
}
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
#include <Adafruit_MLX90614.h>
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
struct {
  bool status;
  float ambient_t;
  float object_t;
} mlxData;

void updateMLX() {
  if (mlxData.status || (mlxData.status = mlx.begin())) {
    mlxData.ambient_t = mlx.readAmbientTempC() / 100.0;
    mlxData.object_t  = mlx.readObjectTempC() / 100.0;
  }
  else {
    mlxData.status = false;
    Serial.println("MLX sensor initialization FAILED!");
  }
}

void serializeMLX(JsonDocument & doc) {

  JsonObject data = doc.createNestedObject("MLX90614");
  data["Ta"] = mlxData.ambient_t;
  data["To"] = mlxData.object_t;
}
#endif //USE_MLX_SENSOR

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

}

void loop() {

  StaticJsonDocument < 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) > weatherDoc;

#ifdef USE_BME_SENSOR
  updateBME();
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

#ifdef USE_MLX_SENSOR
  updateMLX();
  serializeMLX(weatherDoc);
#endif //USE_MLX_SENSOR

  serializeJson(weatherDoc, Serial);
  Serial.println();

  delay(1000);
  //Serial.print("free Memory="); Serial.println(freeMemory());
}
