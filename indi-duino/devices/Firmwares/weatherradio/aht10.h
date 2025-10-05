/*  Streaming functions for the AHT sensor.

    Copyright (C) 2025 Mathias Frankenbach <trifid16@arcor.de> and
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht10;

struct {
  float temperature;
  float humidity;
  bool status;
} aht10Data;

void initAHT10(){
 if (!aht10.begin()) {
    aht10Data.status = false;
    Serial.println("AHT10 not found");
  } else {
    aht10Data.status = true;
  } 
} 

void updateAHT10() {
  sensors_event_t humidity, temp;
  if (aht10.getEvent(&humidity, &temp)) {
    aht10Data.temperature = temp.temperature;
    aht10Data.humidity = humidity.relative_humidity;
    aht10Data.status = true;
  } else {
    aht10Data.status = false;
  }
}

void serializeAHT10(JsonObject& doc) {
  if (!aht10Data.status) return;
  JsonObject data = doc.createNestedObject("AHT10");
  data["init"] = aht10Data.status;

  if (aht10Data.status) {
    data["Temp"] = aht10Data.temperature;
    data["Hum"] = aht10Data.humidity;
  }
}


String displayAHT10Parameters() {
  if (aht10Data.status == false) return "";
  
  return " Temp: " + String(aht10Data.temperature, 1) + " °C\n Hum: " + String(aht10Data.humidity, 1) + "%\n";
}
