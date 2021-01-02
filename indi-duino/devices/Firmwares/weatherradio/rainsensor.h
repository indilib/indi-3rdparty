/*  Streaming functions for rain sensors
    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    Developed on basis of the hookup guide from cactus.io
    http://cactus.io/hookups/weather/rain/hydreon/hookup-arduino-to-hydreon-rg-11-rain-sensor

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

struct rainsensor_data {
  bool status;
  unsigned long lastInterrupt;  // last time an event has been registered
  unsigned int count;           // counter for "bucket full" events
  unsigned int lastcount;       // last counter value that has been processed
  float rainfall;               // measured rain fall in mm
};

volatile rainsensor_data rainsensor_status = {false, 0, 0, 0, 0.0};

// function that the interrupt calls to increment the rain bucket counter
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_rainbucket_full () {
#else
void isr_rainbucket_full () {
#endif

  unsigned long now = millis();
  if ((now - rainsensor_status.lastInterrupt) > 200 ) { // debounce the switch contact.
    rainsensor_status.lastInterrupt = now;
    rainsensor_status.count++;
    Serial.print("Rain bucket full, count = "); Serial.println(rainsensor_status.count);
  }
}


void resetRainSensor() {
  // clear the status
  rainsensor_status.count = 0;
  rainsensor_status.lastcount = 0;
  rainsensor_status.lastInterrupt = millis();
  rainsensor_status.rainfall = 0.0;
}

void initRainSensor() {
  pinMode(RAINSENSOR_PIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(RAINSENSOR_PIN), isr_rainbucket_full, FALLING);
  rainsensor_status.status = true;
  // reset measuring data
  resetRainSensor();
}


void updateRainSensor() {
  if (rainsensor_status.lastcount < rainsensor_status.count) {
    rainsensor_status.rainfall += RAINSENSOR_BUCKET_SIZE * (rainsensor_status.count - rainsensor_status.lastcount);
    rainsensor_status.lastcount = rainsensor_status.count;
  }
}



void serializeRainSensor(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("Rain Sensor");
  data["init"] = rainsensor_status.status;

  if (rainsensor_status.status) {
    data["rainfall"] = rainsensor_status.rainfall;
    data["count"] = rainsensor_status.count;
  }
}

String displayRainSensorParameters() {
  if (rainsensor_status.status == false) return "";

  String result = " rainfall: " + String(rainsensor_status.rainfall, 4) + " \n";
  return result;
}
