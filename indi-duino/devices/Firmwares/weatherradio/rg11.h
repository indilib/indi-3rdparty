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

#define RG11_RAINSENSOR_INTERVAL_LENGTH 60000 // interval for drop counts (ms)

rainsensor_data rg11_rainsensor_status = {false, RG11_MODE, 0, 0, 0, 0, 0.0, 0.0};

// function that the interrupt calls to increment the rain event counter
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_rg11_rain_event () {
#else
void isr_rg11_rain_event () {
#endif

  rain_event(rg11_rainsensor_status);
}


void rg11_resetRainSensor() {
  // clear the status
  resetRainSensor(rg11_rainsensor_status);
}

void rg11_initRainSensor() {
  rg11_rainsensor_status.mode = RG11_MODE;
  pinMode(RG11_RAINSENSOR_PIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(RG11_RAINSENSOR_PIN), isr_rg11_rain_event, FALLING);
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
