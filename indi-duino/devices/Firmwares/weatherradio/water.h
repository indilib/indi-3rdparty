/*  Functions for water detection using a rain sensor such as  
    https://create.arduino.cc/projecthub/MisterBotBreak/how-to-use-a-rain-sensor-bcecd9

    Copyright (C) 2021 Mark Cheverton <mark.cheverton@ecafe.org>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Resistance of sensor is reported from the analog pin and normalised as waterData.wetness
    0 is dryest 1 is wettest
*/

struct {
  bool status;
  float wetness;
} waterData = {false, 0.0};

void updatewater() {
  if (waterData.status == false) {
    pinMode(WATER_PIN, INPUT);
    waterData.status = true;
  }

  waterData.wetness = 100 - 100*float(analogRead(WATER_PIN))/1023;
}

void serializewater(JsonDocument &doc) {
  JsonObject data = doc.createNestedObject("Water");
  data["init"] = waterData.status;

  if (waterData.status) {
    data["wetness"] = waterData.wetness;
  }
}

String displayWaterSensorParameters() {
  if (waterData.status == false) return "";

  String result = " rain: " + String(waterData.wetness, 1) + " % \n";
  return result;
}
