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

#ifdef ESP8266
#include "esp8266.h"
#endif

/**
   Send current sensor data as JSON document
*/
String getSensorData() {
  const int docSize = JSON_OBJECT_SIZE(6) + // max 6 sensors
                      JSON_OBJECT_SIZE(4) + // BME280 sensor
                      JSON_OBJECT_SIZE(3) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // MLX90614 sensor
                      JSON_OBJECT_SIZE(3) + // TSL237 sensor
                      JSON_OBJECT_SIZE(7) + // TSL2591 sensor
                      JSON_OBJECT_SIZE(6);  // Davis Anemometer
  StaticJsonDocument < docSize > weatherDoc;

#ifdef USE_DAVIS_SENSOR
  updateAnemometer();
  serializeAnemometer(weatherDoc);
#endif //USE_DAVIS_SENSOR

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

#ifdef USE_TSL237_SENSOR
  updateTSL237();
  serializeTSL237(weatherDoc);
#endif //USE_TSL237_SENSOR

#ifdef USE_TSL2591_SENSOR
  updateTSL2591();
  serializeTSL2591(weatherDoc);
#endif //USE_TSL2591_SENSOR

  String result = "";
  serializeJson(weatherDoc, result);

  return result;
}

String getCurrentVersion() {
  StaticJsonDocument <JSON_OBJECT_SIZE(1)> doc;
  doc["version"] = METEORADIO_VERSION;

  String result = "";
  serializeJson(doc, result);

  return result;
}

// translate the sensor configurations to a JSON document
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(2) + // max 2 configurations
                      JSON_OBJECT_SIZE(2) + // DHT sensors
                      JSON_OBJECT_SIZE(3);  // Davis Anemometer
  StaticJsonDocument <docSize> doc;
#ifdef USE_DHT_SENSOR
  JsonObject dhtdata = doc.createNestedObject("DHT");
  dhtdata["pin"] = DHTPIN;
  dhtdata["type"] = DHTTYPE;
#endif

#ifdef USE_DAVIS_SENSOR
  JsonObject davisdata = doc.createNestedObject("Davis Anemometer");
  davisdata["wind speed pin"] = WINDSPEEDPIN;
  davisdata["wind direction pin"] = WINDDIRECTIONPIN;
  davisdata["wind direction offset"] = WINDOFFSET;
#endif

#ifdef ESP8266
  JsonObject wifidata = doc.createNestedObject("WiFi");
  wifidata["SSID"] = STASSID;
  wifidata["connected"] = (WiFi.status() == WL_CONNECTED);
  if (WiFi.status() == WL_CONNECTED)
    wifidata["IP"] = WiFi.localIP().toString();
#endif

  if (doc.isNull())
    return "{}";
  else {
    String result = "";
    serializeJson(doc, result);

    return result;
  };
}

#ifdef ESP8266
void handleSensorData() {
  server.send(200, "application/json; charset=utf-8", getSensorData());

}
#endif

void setup() {
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

#ifdef USE_DAVIS_SENSOR
  initAnemometer();
#endif //USE_DAVIS_SENSOR

#ifdef USE_TSL237_SENSOR
  initTSL237();
#endif //USE_TSL237_SENSOR


#ifdef ESP8266
  initWiFi();

  server.on("/", []() {
    server.send(200, "application/json; charset=utf-8", getSensorData());
  });

  server.on("/w", []() {
    server.send(200, "application/json; charset=utf-8", getSensorData());
  });

  server.on("/c", []() {
    server.send(200, "application/json; charset=utf-8", getCurrentConfig());
  });

  server.on("/v", []() {
    server.send(200, "application/json; charset=utf-8", getCurrentVersion());
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Ressource not found: " + server.uri());
  });

  server.begin();
#endif
}

/**
   Command loop handling incoming requests and returns a JSON document.

   'v' - send current version
   'w' - send current weather sensor values
   'c' - send sensor configuration settings
*/
void loop() {

#ifdef ESP8266
  server.handleClient();
#endif

  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'v':
        Serial.println(getCurrentVersion());
        break;
      case 'w':
        Serial.println(getSensorData());
        break;
      case 'c':
        Serial.println(getCurrentConfig());
    }
  }

  delay(50);
}
