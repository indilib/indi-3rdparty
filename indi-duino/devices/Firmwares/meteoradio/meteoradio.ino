
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


void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

}

void loop() {

  StaticJsonDocument <JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3)> weatherDoc;

#ifdef USE_BME_SENSOR
  updateBME();
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

  serializeJson(weatherDoc, Serial);
  Serial.println();

  delay(1000);
  //Serial.print("free Memory="); Serial.println(freeMemory());
}
