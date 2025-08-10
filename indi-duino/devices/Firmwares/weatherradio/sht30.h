
/*  Streaming functions for the SHT sensors.

    Copyright (C) 2025 Mathias Frankenbach <trifid16@arcor.de> and
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <Wire.h>
#include <Adafruit_SHT31.h>

Adafruit_SHT31 sht30 = Adafruit_SHT31();

struct {
  float temperature;
  float humidity;
  bool status;
} sht30Data;

void initSHT30(){
 if (!sht30.begin(0x44)) { // standard address SHT30
    sht30Data.status = false;
    Serial.println("SHT30 sensor not found");
  } else {
    sht30Data.status = true;
  } 
} 

void updateSHT30() {
//  sensors_event_t humidity, temp;
    if (sht30.begin(0x44)) { 
      sht30Data.temperature = sht30.readTemperature();
      sht30Data.humidity = sht30.readHumidity();
      sht30Data.status = true;
    } else {
      sht30Data.status = false;
    }
}

void serializeSHT30(JsonObject& doc) {
  if (!sht30Data.status) return;
  JsonObject sht = doc.createNestedObject("SHT30");
  sht["temperature"] = sht30Data.temperature;
  sht["humidity"] = sht30Data.humidity;
}

String displaySHT30Parameters() {
  if (sht30Data.status == false) return "";
  
  return " Temp: " + String(sht30Data.temperature, 1) + " °C\n Hum: " + String(sht30Data.humidity, 1) + "%\n";
}
