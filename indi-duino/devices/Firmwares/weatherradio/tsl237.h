/*  Measuring functions for the TSL 237 light sensor.

    Copyright (C) 2020 Allesandro Pensato,
                       Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

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
int SAMPLING_INTERVAL = 2000; // sampling distance between two frequency measurements
int AVERAGE_COUNT = 5;        // amount of measurements where the average is calculated

struct {
  bool          status;
  int           count;
  unsigned long lastMeasure; // Last time the frequency had been measured
  double        sum;
  double        frequency;
  float         sqm;
} tsl237Data {false, 0, 0, 0.0, 0.0, 0.0};


void initTSL237() {
  FreqMeasure.begin();
  tsl237Data.lastMeasure = millis();
}

void updateTSL237() {
  if (tsl237Data.status || (tsl237Data.status = FreqMeasure.available())) {
    volatile unsigned long now = millis();
    if ( ((now - tsl237Data.lastMeasure) > SAMPLING_INTERVAL)) {
      tsl237Data.sum += FreqMeasure.read();
      tsl237Data.count++;
      tsl237Data.lastMeasure = now;

      if (tsl237Data.count >= AVERAGE_COUNT) {
        tsl237Data.frequency = FreqMeasure.countToFrequency(tsl237Data.sum / tsl237Data.count);
        tsl237Data.sqm = A - 2.5 * log10(tsl237Data.frequency); //Egen Kod
        if(tsl237Data.sqm >= 20){
            SAMPLING_INTERVAL = 6000;
        } else {
          SAMPLING_INTERVAL = 2000;
        }

        // reset the counters
        tsl237Data.sum = 0;
        tsl237Data.count = 0;
      }
    }
  }
}

void serializeTSL237(JsonObject &doc) {

  JsonObject data = doc.createNestedObject("TSL237");
  data["init"] = tsl237Data.status;

  if (tsl237Data.status) {
    data["SQM"]       = tsl237Data.sqm;
    data["Frequency"] = tsl237Data.frequency;
  }
}

String displayTSL237Parameters() {
  if (tsl237Data.status == false) return "";
  
  return " SQM: " + String(tsl237Data.sqm, 1) + "\n Frequency: " + String(tsl237Data.frequency, 1) + "\n";
}
