/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Based upon ideas from indiduinoMETEO (http://indiduino.wordpress.com).
*/


#include <ArduinoJson.h>

/* ********************************************************************

  Edit config.h to configure the attached sensors

*********************************************************************** */
#include "config.h"
#include "version.h"

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

}

/**
   Send current sensor data as JSON document
*/
void sendSensorData() {
  const int docSize = JSON_OBJECT_SIZE(4) + // max 4 sensors
                      JSON_OBJECT_SIZE(4) + // BME280 sensor
                      JSON_OBJECT_SIZE(3) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // MLX90614 sensor
                      JSON_OBJECT_SIZE(6);  // TSL2591 sensor
  StaticJsonDocument < docSize > weatherDoc;

#ifdef USE_BME_SENSOR
  updateBME();
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

#ifdef USE_DHT_SENSOR
  updateDHT();
  serializeDHT(weatherDoc);
#endif //USE_DHT_SENSOR

#ifdef USE_MLX_SENSOR
  updateMLX();
  serializeMLX(weatherDoc);
#endif //USE_MLX_SENSOR

#ifdef USE_TSL_SENSOR
  updateTSL();
  serializeTSL(weatherDoc);
#endif //USE_TSL_SENSOR

  serializeJson(weatherDoc, Serial);
  Serial.println();

}

void sendCurrentVersion() {
  StaticJsonDocument <JSON_OBJECT_SIZE(1)> doc;
  doc["version"] = METEORADIO_VERSION;

  serializeJson(doc, Serial);
  Serial.println();
}

/**
   Command loop handling incoming requests and returns a JSON document.

   'v' - send current version
   'w' - send current weather sensor values
*/
void loop() {


  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'v':
        sendCurrentVersion();
        break;
      case 'w':
        sendSensorData();
        break;
    }
  }

  delay(50);
}
