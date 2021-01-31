/*  Abstract streaming functions for all types of rain sensors.
    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    Developed on basis of the hookup guide from cactus.io
    http://cactus.io/hookups/weather/rain/hydreon/hookup-arduino-to-hydreon-rg-11-rain-sensor

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#if !defined(RAINSENSOR_H)
#define RAINSENSOR_H
struct rainsensor_data {
  bool status;
  unsigned long lastInterrupt;  // last time an event has been registered
  unsigned long startMeasuring; // start time of the measuring interval
  unsigned int intervalCount;   // counter for "bucket full" events since startMeasuring
  unsigned int count;           // overall counter for "bucket full" events
  float rain_intensity;         // current measured rain fall in mm/h
  float rain_volume;            // overall rain fall measured in mm
};


// function that the interrupt calls to increment the rain bucket counter
void rainbucket_full (rainsensor_data &data) {

  unsigned long now = millis();
  if ((now - data.lastInterrupt) > 200 ) { // debounce the switch contact.
    data.lastInterrupt = now;
    data.intervalCount++;
    Serial.print("Rain bucket full, count="); Serial.println(data.intervalCount);
  }
}


void resetRainSensor(rainsensor_data &data) {
  // clear the status
  data.lastInterrupt = millis();
  data.startMeasuring = data.lastInterrupt;
  data.intervalCount = 0;
  data.count = 0;
  data.rain_intensity = 0.0;
  data.rain_volume = 0.0;
}

void updateRainSensor(rainsensor_data &data, unsigned long interval_length, int bucket_size) {
  unsigned long now = millis();
  unsigned long elapsed = now - data.startMeasuring;
  if (elapsed > interval_length) {
    // measuring interval over, update event counter
    data.count += data.intervalCount;
    // calculate rain intensity: bucket_size * count scaled up from elapsed time to 1h
    data.rain_intensity = bucket_size * data.intervalCount * 3600000 / elapsed;
    // update total rain fall volume
    data.rain_volume += bucket_size * data.intervalCount;
    // clear interval data
    data.startMeasuring = now;
    data.intervalCount = 0;
  }
}



void serializeRainSensor(JsonDocument &doc, rainsensor_data &data, String name) {

  JsonObject json = doc.createNestedObject(name);
  json["init"] = data.status;

  if (data.status) {
    json["rain intensity"] = data.rain_intensity;
    json["rain volume"]    = data.rain_volume;
    json["count"]          = data.count;
  }
}

String displayRainSensorParameters(rainsensor_data &data) {
  if (data.status == false) return "";

  String result = " rain: " + String(data.rain_intensity, 3) + " mm/h \n";
  result += " rain vol: " + String(data.rain_volume, 3) + " mm \n";
  return result;
}
#endif //!defined(RAINSENSOR_H)
