/*  streaming functions for the Ventus W174 rain sensor.
    Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    Developed on basis of the hookup guide from cactus.io
    http://cactus.io/hookups/weather/rain/hydreon/hookup-arduino-to-hydreon-rg-11-rain-sensor

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "rainsensor.h"

#define W174_RAINSENSOR_INTERVAL_LENGTH 60000 // interval for a single speed mesure (ms)

rainsensor_data w174_rainsensor_status = {false, 0, 0, 0, 0, 0.0, 0.0};

// function that the interrupt calls to increment the rain bucket counter
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_w174_rainbucket_full () {
#else
void isr_w174_rainbucket_full () {
#endif

  rainbucket_full(w174_rainsensor_status);
}


void w174_resetRainSensor() {
  // clear the status
  resetRainSensor(w174_rainsensor_status);
}

void w174_initRainSensor() {
  pinMode(W174_RAINSENSOR_PIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(W174_RAINSENSOR_PIN), isr_w174_rainbucket_full, FALLING);
  w174_rainsensor_status.status = true;
  // reset measuring data
  w174_resetRainSensor();
}


void w174_updateRainSensor() {
  updateRainSensor(w174_rainsensor_status, W174_RAINSENSOR_INTERVAL_LENGTH, W174_RAINSENSOR_BUCKET_SIZE);
}



void w174_serializeRainSensor(JsonDocument &doc) {
  serializeRainSensor(doc, w174_rainsensor_status, "W174 Rain Sensor");
}

String w174_displayRainSensorParameters() {
  return displayRainSensorParameters(w174_rainsensor_status);
}
