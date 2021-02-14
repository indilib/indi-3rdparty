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
  unsigned int mode;            // 0 = tipping bucket mode, 1 = drop detection
  unsigned long lastInterrupt;  // last time an event has been registered
  unsigned long startMeasuring; // start time of the measuring interval
  unsigned int intervalCount;   // counter for "bucket full" events since startMeasuring
  unsigned int count;           // overall counter for "bucket full" or "drop detected" events
  float rainVolume;             // overall rain fall measured in mm
  unsigned int eventFrequency;  // frequency of rain events ("bucket full" or "drop detected") in events/min
};


// function that the interrupt calls to increment the rain event counter
void rain_event (rainsensor_data &data) {

  unsigned long now = millis();
  if ((now - data.lastInterrupt) > 200 ) { // debounce the switch contact.
    data.lastInterrupt = now;
    data.intervalCount++;
  }
}


void resetRainSensor(rainsensor_data &data) {
  // clear the status
  data.lastInterrupt = millis();
  data.startMeasuring = data.lastInterrupt;
  data.intervalCount = 0;
  data.count = 0;
  data.rainVolume = 0.0;
}

void updateRainSensor(rainsensor_data &data, unsigned long interval_length, float bucket_size) {
  unsigned long now = millis();
  unsigned long elapsed = now - data.startMeasuring;
  if (elapsed > interval_length) {
    // measuring interval over, update event counter
    data.count += data.intervalCount;
    // update total rain fall volume
    data.rainVolume += bucket_size * data.intervalCount;
    // update event frequency
    data.eventFrequency = round(data.intervalCount * 60000.0 / elapsed);
    // clear interval data
    data.startMeasuring = now;
    data.intervalCount = 0;
  }
}



void serializeRainSensor(JsonDocument &doc, rainsensor_data &data, String name) {

  JsonObject json = doc.createNestedObject(name);
  json["init"] = data.status;
  json["mode"] = data.mode == 0 ? "tipping bucket" : "drop detect";

  if (data.status) {
    json["count"] = data.count;
    // only relevant in tipping bucket mode
    if (data.mode == 0) {
      json["rain volume"] = data.rainVolume;
    } else {
      json["drop freq"] = data.eventFrequency;

    }
  }
}

String displayRainSensorParameters(rainsensor_data &data) {
  String result = "";
  if (data.status == false) return result;

  if (data.mode == 0)
    result += " rain vol: " + String(data.rainVolume, 3) + " mm \n";
  else
    result += " drop count: " + String(data.count) + " \n";

  return result;
}
#endif //!defined(RAINSENSOR_H)
