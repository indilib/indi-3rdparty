/*  streaming functions for the RG-11 rain sensor.
    Copyright (C) 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    Developed on basis of the hookup guide from cactus.io
    http://cactus.io/hookups/weather/rain/hydreon/hookup-arduino-to-hydreon-rg-11-rain-sensor

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "rainsensor.h"

#define RG11_RAINSENSOR_INTERVAL_LENGTH 60000 // interval for a single speed mesure (ms)

rainsensor_data rg11_rainsensor_status = {false, 0, 0, 0, 0, 0.0, 0.0};

// function that the interrupt calls to increment the rain bucket counter
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_rg11_rainbucket_full () {
#else
void isr_rg11_rainbucket_full () {
#endif

  rainbucket_full(rg11_rainsensor_status);
}


void rg11_resetRainSensor() {
  // clear the status
  resetRainSensor(rg11_rainsensor_status);
}

void rg11_initRainSensor() {
  pinMode(RG11_RAINSENSOR_PIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(RG11_RAINSENSOR_PIN), isr_rg11_rainbucket_full, FALLING);
  rg11_rainsensor_status.status = true;
  // reset measuring data
  rg11_resetRainSensor();
}


void rg11_updateRainSensor() {
  updateRainSensor(rg11_rainsensor_status, RG11_RAINSENSOR_INTERVAL_LENGTH, RG11_RAINSENSOR_BUCKET_SIZE);
}



void rg11_serializeRainSensor(JsonDocument &doc) {
  serializeRainSensor(doc, rg11_rainsensor_status, "RG11 Rain Sensor");
}

String rg11_displayRainSensorParameters() {
  return displayRainSensorParameters(rg11_rainsensor_status);
}
