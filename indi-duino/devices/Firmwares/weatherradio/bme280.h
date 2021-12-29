/*  Streaming functions for the BME280 environment sensor.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "Adafruit_BME280.h"
Adafruit_BME280 bme;
struct {
  bool status;
  float temperature;
  float pressure;
  float humidity;
} bmeData;


void updateBME() {
  if (bmeData.status || (bmeData.status = bme.begin()) || (bmeData.status = bme.begin(BME280_ADDRESS_ALTERNATE))) {
    bmeData.temperature = bme.readTemperature();
    bmeData.pressure    = bme.readPressure() / 100.0;
    bmeData.humidity    = bme.readHumidity();
  }
}

void serializeBME(JsonObject &doc) {

  JsonObject data = doc.createNestedObject("BME280");
  data["init"] = bmeData.status;

  if (bmeData.status) {
    data["Temp"] = bmeData.temperature;
    data["Pres"] = bmeData.pressure;
    data["Hum"] = bmeData.humidity;
  }
}

String displayBMEParameters() {
  if (bmeData.status == false) return "";

  return " Temp: " + String(bmeData.temperature, 1) + "\n Pres: " + String(bmeData.pressure, 1) + "\n Hum: " + String(bmeData.humidity, 1) + "\n";
}
