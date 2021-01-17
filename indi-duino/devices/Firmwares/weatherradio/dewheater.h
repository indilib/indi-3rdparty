/*  Functions for control of a dew heater
 *  Assuming a simple resistor network such as: 
 *  https://www.dewcontrol.com/Dew_Heater_Module_-_All_Sky_Camera/p3099125_19045496.aspx

    Copyright (C) 2021 Mark Cheverton <mark.cheverton@ecafe.org>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Dew heater is attached via a relay controlled by DEW_PIN
*/

#include <math.h>

struct {
  bool status;
  bool heater_status;
  float dew_temp;
} dewheaterData = {false, false, 0.0};

// calculates dew point
// input:   humidity [%RH], temperature in C
// output:  dew point in C
float calc_dewpoint(float t, float h) {
  float logEx, dew_point;
  logEx = 0.66077 + 7.5 * t / (237.3 + t) + (log10(h) - 2);
  dew_point = (logEx - 0.66077) * 237.3 / (0.66077 + 7.5 - logEx);
  return dew_point;
}

void updateDewheater(float t, float h) {
  dewheaterData.dew_temp = calc_dewpoint(t, h);
  
  // Dew occurs when the ambient tempererature meets the dew point
  // Equipment is likely to be colder than ambient so use a delta
  if ( (t <  dewheaterData.dew_temp + DEWHEATER_MAX_DELTA) || (t < DEWHEATER_MIN_TEMPERATURE) ) {
    digitalWrite(DEWHEATER_PIN, HIGH);
    dewheaterData.heater_status = true;
  } else {
    digitalWrite(DEWHEATER_PIN, LOW);
    dewheaterData.heater_status = false;
  }
}

void updateDewheater() {
#ifdef USE_DHT_SENSOR
  updateDewheater(dhtData.temperature, dhtData.humidity);
#endif // USE_DHT_SENSOR
#ifdef USE_BME_SENSOR
  updateDewheater(bmeData.temperature, bmeData.humidity);
#endif // USE_BME_SENSOR
}

void serializeDewheater(JsonDocument &doc) {
  JsonObject data = doc.createNestedObject("Dew Heater");
  
  data["pin"] = DEWHEATER_PIN;
  data["min temp"] = DEWHEATER_MIN_TEMPERATURE;
  data["max delta"] = DEWHEATER_MAX_DELTA;
  data["status"] = dewheaterData.heater_status;
  data["dew temp"] = dewheaterData.dew_temp;
}

void initDewheater() {
  dewheaterData.status = true;
  pinMode(DEWHEATER_PIN, OUTPUT);
}
