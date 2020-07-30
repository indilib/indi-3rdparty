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

// sensor measuring duration
struct {
  unsigned long bme_read;
  unsigned long dht_read;
  unsigned long mlx90614_read;
  unsigned long tsl237_read;
  unsigned long tsl2591_read;
  unsigned long davis_read;
} sensor_read;


/**
   Update all sensor data
*/
void updateSensorData() {

  // reset all timers
  sensor_read = { 0, 0, 0, 0, 0, 0};
  unsigned long start = 0;

#ifdef USE_DAVIS_SENSOR
  start = millis();
  readAnemometer();
  sensor_read.davis_read = millis() - start;
#endif //USE_DAVIS_SENSOR

#ifdef USE_BME_SENSOR
  start = millis();
  updateBME();
  sensor_read.bme_read = millis() - start;
#endif //USE_BME_SENSOR

#ifdef USE_DHT_SENSOR
  start = millis();
  updateDHT();
  sensor_read.dht_read = millis() - start;
#endif //USE_DHT_SENSOR

#ifdef USE_MLX_SENSOR
  start = millis();
  updateMLX();
  sensor_read.mlx90614_read = millis() - start;
#endif //USE_MLX_SENSOR

#ifdef USE_TSL237_SENSOR
  start = millis();
  updateTSL237();
  sensor_read.tsl237_read = millis() - start;
#endif //USE_TSL237_SENSOR

#ifdef USE_TSL2591_SENSOR
  start = millis();
  updateTSL2591();
  sensor_read.tsl2591_read = millis() - start;
#endif //USE_TSL2591_SENSOR
}

/**
   Send current sensor data as JSON document to Serial
*/
String getSensorData(bool pretty) {
  const int docSize = JSON_OBJECT_SIZE(6) + // max 6 sensors
                      JSON_OBJECT_SIZE(1) + // token data
                      JSON_OBJECT_SIZE(4) + // BME280 sensor
                      JSON_OBJECT_SIZE(3) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // MLX90614 sensor
                      JSON_OBJECT_SIZE(3) + // TSL237 sensor
                      JSON_OBJECT_SIZE(7) + // TSL2591 sensor
                      JSON_OBJECT_SIZE(6);  // Davis Anemometer
  StaticJsonDocument < docSize > weatherDoc;

  unsigned long start = 0;

#ifdef USE_DAVIS_SENSOR
  serializeAnemometer(weatherDoc);
#endif //USE_DAVIS_SENSOR

#ifdef USE_BME_SENSOR
  serializeBME(weatherDoc);
#endif //USE_BME_SENSOR

#ifdef USE_DHT_SENSOR
  serializeDHT(weatherDoc);
#endif //USE_DHT_SENSOR

#ifdef USE_MLX_SENSOR
  serializeMLX(weatherDoc);
#endif //USE_MLX_SENSOR

#ifdef USE_TSL237_SENSOR
  serializeTSL237(weatherDoc);
#endif //USE_TSL237_SENSOR

#ifdef USE_TSL2591_SENSOR
  serializeTSL2591(weatherDoc);
#endif //USE_TSL2591_SENSOR

  String result = "";
  if (pretty)
    serializeJsonPretty(weatherDoc, result);
  else
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

String getReadDurations() {
  StaticJsonDocument <JSON_OBJECT_SIZE(6)> doc;
#ifdef USE_BME_SENSOR
  if (bmeData.status)        doc["BME"]              = sensor_read.bme_read;
#endif //USE_BME_SENSOR
#ifdef USE_DHT_SENSOR
  if (dhtData.status)        doc["DHT"]              = sensor_read.dht_read;
#endif //USE_DHT_SENSOR
#ifdef USE_MLX_SENSOR
  if (mlxData.status)        doc["MLX90614"]         = sensor_read.mlx90614_read;
#endif //USE_MLX_SENSOR
#ifdef USE_TSL237_SENSOR
  if (tsl237Data.status)     doc["TSL237"]           = sensor_read.tsl237_read;
#endif //USE_TSL237_SENSOR
#ifdef USE_TSL2591_SENSOR
  if (tsl2591Data.status)    doc["TSL2591"]          = sensor_read.tsl2591_read;
#endif //USE_TSL2591_SENSOR
#ifdef USE_DAVIS_SENSOR
  if (anemometerData.status) doc["Davis Anemometer"] = sensor_read.davis_read;
#endif //USE_DAVIS_SENSOR

  String result = "";
  serializeJson(doc, result);

  return result;
}


// translate the sensor configurations to a JSON document
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(4) + // max 4 configurations
                      JSON_OBJECT_SIZE(2) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // Davis Anemometer
                      JSON_OBJECT_SIZE(3) + // WiFi parameters
                      JSON_OBJECT_SIZE(1) + // Arduino
                      JSON_OBJECT_SIZE(2);  // buffer
  StaticJsonDocument <docSize> doc;

#ifdef USE_WIFI
  // currently, we have memory info only available for ESP8266
  JsonObject arduinodata = doc.createNestedObject("Arduino");
  arduinodata["free memory"] = freeMemory();
#endif

#ifdef USE_DHT_SENSOR
  JsonObject dhtdata = doc.createNestedObject("DHT");
  dhtdata["pin"]  = DHTPIN;
  dhtdata["type"] = DHTTYPE;
#endif

#ifdef USE_DAVIS_SENSOR
  JsonObject davisdata               = doc.createNestedObject("Davis Anemometer");
  davisdata["wind speed pin"]        = ANEMOMETER_WINDSPEEDPIN;
  davisdata["wind direction pin"]    = ANEMOMETER_WINDDIRECTIONPIN;
  davisdata["wind direction offset"] = ANEMOMETER_WINDOFFSET;
#endif

#ifdef USE_WIFI
  JsonObject wifidata = doc.createNestedObject("WiFi");
  wifidata["SSID"] = esp8266Data.ssid;
  wifidata["connected"] = WiFi.status() == WL_CONNECTED;
  if (WiFi.status() == WL_CONNECTED)
    wifidata["IP"]        = WiFi.localIP().toString();
  else
    wifidata["IP"]        = "";
#endif

  String result = "";
  serializeJson(doc, result);

  if (doc.isNull())
    return "{}";
  else {
    return result;
  }
}

unsigned long lastSensorRead;

void setup() {
  Serial.begin(BAUD_RATE);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

  // sensors never read
  lastSensorRead = 0;

#ifdef USE_DAVIS_SENSOR
  initAnemometer();
#endif //USE_DAVIS_SENSOR

#ifdef USE_TSL237_SENSOR
  initTSL237();
#endif //USE_TSL237_SENSOR

#ifdef USE_WIFI
  initWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    server.on("/", []() {
      server.send(200, "application/json; charset=utf-8", getSensorData(false));
    });

    server.on("/w", []() {
      server.send(200, "application/json; charset=utf-8", getSensorData(false));
    });

    server.on("/p", []() {
      server.send(200, "application/json; charset=utf-8", getSensorData(true));
    });

    server.on("/c", []() {
      server.send(200, "application/json; charset=utf-8", getCurrentConfig());
    });

    server.on("/v", []() {
      server.send(200, "application/json; charset=utf-8", getCurrentVersion());
    });

    server.on("/r", []() {
      reset();
      server.send(200, "application/json; charset=utf-8", getCurrentVersion());
    });

    server.on("/t", []() {
      server.send(200, "application/json; charset=utf-8", getReadDurations());
    });

    server.onNotFound([]() {
      server.send(404, "text/plain", "Ressource not found: " + server.uri());
    });

    server.begin();

  }
#endif

  // initial readout all sensors
  updateSensorData();

}

String input = "";

void parseInput() {
  // ignore empty input
  if (input.length() == 0)
    return;

  switch (input.charAt(0)) {
    case 'v':
      Serial.println(getCurrentVersion());
      break;
    case 'w':
      Serial.println(getSensorData(false));

      break;
    case 'c':
      Serial.println(getCurrentConfig());
      break;
    case 'p':
      Serial.println(getSensorData(true));

      break;
    case 't':
      Serial.println(getReadDurations());
      break;
#ifdef USE_WIFI
    case 's':
      if (input.length() > 2 && input.charAt(1) == '?')
        parseCredentials(input.substring(2));
      disconnectWiFi();
      initWiFi();
      break;
    case 'd':
      disconnectWiFi();
      break;
    case 'r':
      reset();
      break;
#endif
  }

}

/**
   Command loop handling incoming requests and returns a JSON document.

   'v' - send current version
   'w' - send current weather sensor values
   'p' - send current weather sensor values (pretty printed)
   't' - send sensor read durations
   'c' - send sensor configuration settings
*/
void loop() {

  byte ch;
  String valStr;
  int val;

#ifdef USE_WIFI
  wifiServerLoop();
#endif

#ifdef USE_TSL237_SENSOR
  updateTSL237();
#endif //USE_TSL237_SENSOR

#ifdef USE_DAVIS_SENSOR
  updateAnemometer();
#endif //USE_DAVIS_SENSOR

  if (Serial.available() > 0) {
    ch = Serial.read();

    if (ch == '\r' || ch == '\n') { // Command received and ready.
      parseInput();
      input = "";
    }
    else
      input += (char)ch;
  }

  // regularly update sensor data
  unsigned long now = millis();
  if (abs(now - lastSensorRead) > MAX_CACHE_AGE) {
    updateSensorData();
    lastSensorRead = now;
  }

  delay(50);
}
