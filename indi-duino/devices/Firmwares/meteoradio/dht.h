/*  Streaming functions for the DHT humidity sensor family.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "DHT.h"

#define DHTPIN 3     // Digital pin connected to the DHT sensor

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

struct {
  bool status;
  float temperature;
  float humidity;
} dhtData;

void updateDHT() {
  if (dhtData.status == false) {
    dht.begin();
    dhtData.status = true;
  }
  if (dhtData.status) {
    dhtData.temperature = dht.readTemperature();
    dhtData.humidity    = dht.readHumidity();
  }
  else {
    dhtData.status = false;
    Serial.println("dht sensor initialization FAILED!");
  }
}

void serializeDHT(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("DHT");
  data["Temp"] = dhtData.temperature;
  data["Hum"] = dhtData.humidity;
}
