/*  Streaming functions for the Davis anemometer measuring wind speed and
    direction:
    https://www.davisinstruments.com/product/anemometer-for-vantage-pro2-vantage-pro/

    Developed on basis of the hookup guide from
    http://cactus.io/hookups/weather/anemometer

    The wind speed is measured in m/s, the direction is measured in deg, i.e.
    N = 0 deg, E = 90 deg etc.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include <math.h>

#define SLICEDURATION 5000 // interval for a single speed mesure

struct {
  bool status;
  int direction;
  unsigned int rotations;
  float avgSpeed;
  float minSpeed;
  float maxSpeed;
} anemometerData;

// intermediate values to translate #rotations into wind speed
volatile unsigned long startTime;     // overall start time for calculating the wind speed
volatile unsigned long startSlice;    // start time of the current time slice to measure wind speed
volatile unsigned long lastInterrupt; // Last time a rotation has been detected
volatile unsigned int rotations;      // total number of wind wheel rotations
volatile unsigned int sliceRotations; // rotation occured in the current time slice
volatile unsigned int slices;         // number of slices occured since startTime
volatile float minSpeed;              // minimal wind speed since startTime
volatile float maxSpeed;              // maximal wind speed since startTime

// intermediate values to calculate an average wind direction
volatile float initial_direction; // remember the first direction measured
volatile float direction_diffs;   // and collect the diffs to build the average


// calculate the windspeed
float windspeed(unsigned long time, unsigned long startTime, unsigned int rotations) {

  // 1600 rotations per hour or 2.25 seconds per rotation
  // equals 1 mp/h wind speed (1 mp/h = 1609/3600 m/s)
  // speed (m/s) = rotations * 1135.24 / delta t

  if (time == startTime)
    return 0.0;
  else
    return (rotations * 1135.24 / (time - startTime));
}

// calculate the wind direction in degree (N = 0, E = 90, ...)
int winddirection() {
  // the wind direction is measured with a potentiometer
  volatile int direction = map(analogRead(ANEMOMETER_WINDDIRECTIONPIN), 0, 1023, 0, 360) + ANEMOMETER_WINDOFFSET;

  // ensure 0 <= direction <360
  if (direction >= 360)
    direction -= 360;
  else if (direction < 0)
    direction += 360;

  return direction;
}



// This is the function that the interrupt calls to increment the rotation count
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_rotation () {
#else
void isr_rotation () {
#endif

  volatile unsigned long now = millis();
  if ((now - lastInterrupt) > 15 ) { // debounce the switch contact.
    rotations++;
    sliceRotations++;
    lastInterrupt = now;
  }
}


void reset(unsigned long time) {
  startTime      = time;
  startSlice     = time;
  lastInterrupt  = time;
  rotations      = 0;
  sliceRotations = 0;
  slices         = 0;
  maxSpeed       = 0.0;
  minSpeed       = 9999.0;
  initial_direction = winddirection();
  direction_diffs = 0;
}


void initAnemometer() {
  pinMode(ANEMOMETER_WINDSPEEDPIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_WINDSPEEDPIN), isr_rotation, FALLING);
  anemometerData.status = true;
  // reset measuring data
  reset(millis());
}

/**
   Update anemometer counters
*/
void updateAnemometer() {
  if (anemometerData.status) {
    if ((lastInterrupt > startSlice) && (lastInterrupt - startSlice >= SLICEDURATION)) {
      // stop recording
      detachInterrupt(digitalPinToInterrupt(ANEMOMETER_WINDSPEEDPIN));

      // update wind speed data
      volatile float speed = windspeed(lastInterrupt, startSlice, sliceRotations);

      // update min and max values
      minSpeed = speed < minSpeed ? speed : minSpeed;
      maxSpeed = speed > maxSpeed ? speed : maxSpeed;

      // reset the single interval
      startSlice = millis();
      sliceRotations = 0;
      slices++;

      // calculate the difference in the wind direction
      volatile int current_direction = winddirection();
      volatile int diff = initial_direction - current_direction;
      // ensure that the diff is in the range -180 < diff <= 180
      if (diff > 180) diff -= 360;
      if (diff <= -180) diff += 360;
      direction_diffs += diff;

      // start recording
      attachInterrupt(digitalPinToInterrupt(ANEMOMETER_WINDSPEEDPIN), isr_rotation, FALLING);
    }
  } else
    initAnemometer();

}

/**
   Read out the anemometer data and reset the counters
*/
void readAnemometer() {
  updateAnemometer();
  anemometerData.avgSpeed = windspeed(lastInterrupt, startTime, rotations);
  anemometerData.minSpeed = min(minSpeed, maxSpeed);
  anemometerData.maxSpeed = maxSpeed;
  anemometerData.rotations = rotations;
  if (slices > 0)
    anemometerData.direction = round(initial_direction - (direction_diffs / slices));
  else
    anemometerData.direction = initial_direction;
  reset(millis());
}

void serializeAnemometer(JsonDocument &doc) {

  JsonObject data = doc.createNestedObject("Davis Anemometer");
  data["init"] = anemometerData.status;

  if (anemometerData.status) {
    data["direction"] = anemometerData.direction;
    data["avg speed"] = anemometerData.avgSpeed;
    data["min speed"] = anemometerData.minSpeed;
    data["max speed"] = anemometerData.maxSpeed;
    data["rotations"] = anemometerData.rotations;
  }
}
