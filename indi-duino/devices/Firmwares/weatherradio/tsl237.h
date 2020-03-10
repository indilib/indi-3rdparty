/*  Measuring functions for the TSL 237 light sensor.

    Copyright (C) 2020 Allesandro Pensato, 
                       Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/


#include <stdlib.h>
#include <math.h>
#include <FreqMeasure.h>
#include <Arduino.h>
#include <Wire.h>

const float A = 20.53;

struct {
  bool   status;
  int    count;
  double sum;
  float  frequency;
  float  sqm;
} tsl237Data {false, 0, 0.0, 0.0, 0.0};


void initTSL237() {
  FreqMeasure.begin();
}

void updateTSL237() {
  if (tsl237Data.status || (tsl237Data.status = FreqMeasure.available())) {
    tsl237Data.sum += FreqMeasure.read();
    tsl237Data.count++;

    if (tsl237Data.count > 4) {
      //float frequency = F_CPU / (sum / count);
      tsl237Data.frequency = FreqMeasure.countToFrequency(tsl237Data.sum / tsl237Data.count);
      tsl237Data.sqm = A - 2.5 * log10(tsl237Data.frequency); //Egen Kod

      // reset the counters
      tsl237Data.sum = 0;
      tsl237Data.count = 0;
    }
  }
}

void serializeTSL237(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("TSL237");
  data["init"] = tsl237Data.status;

  if (tsl237Data.status) {
    data["SQM"]       = tsl237Data.sqm;
    data["Frequency"] = tsl237Data.frequency;
  }
}
