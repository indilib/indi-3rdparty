/*  Firmware for a weather sensor device streaming the data as JSON documents.

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

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

// sensor measuring duration
struct {
  unsigned long bme_read;
  unsigned long dht_read;
  unsigned long mlx90614_read;
  unsigned long tsl237_read;
  unsigned long tsl2591_read;
  unsigned long davis_read;
  unsigned long water_read;
  unsigned long rg11_rainsensor_read;
  unsigned long w174_rainsensor_read;
} sensor_read;

void updateDisplayText() {
#ifdef USE_OLED
  String result = "";
  result += "Weather Radio V ";
  result += WEATHERRADIO_VERSION;
  result += " \n \n";
#ifdef USE_WIFI
  result += displayWiFiParameters() + " \n";
#endif //USE_WIFI
#ifdef USE_BME_SENSOR
  result += "BME\n" + displayBMEParameters() + " \n";
#endif //USE_BME_SENSOR
#ifdef USE_DAVIS_SENSOR
  result += "Davis Anemometer\n" + displayAnemometerParameters() + " \n";
#endif //USE_DAVIS_SENSOR
#ifdef USE_DHT_SENSOR
  result += "DHT\n" + displayDHTParameters() + " \n";
#endif //USE_DHT_SENSOR
#ifdef USE_MLX_SENSOR
  result += "MLX90614\n" + displayMLXParameters() + " \n";
#endif //USE_MLX_SENSOR
#ifdef USE_TSL237_SENSOR
  result += "TSL237\n" + displayTSL237Parameters() + " \n";
#endif //USE_TSL237_SENSOR
#ifdef USE_TSL2591_SENSOR
  result += "TSL2591\n" + displayTSL2591Parameters() + " \n";
#endif //USE_TSL2591_SENSOR
#ifdef USE_RG11_RAIN_SENSOR
  result += "RG11 Rain Sensor\n" + rg11_displayRainSensorParameters() + " \n";
#endif //USE_RG11_RAIN_SENSOR
#ifdef USE_W174_RAIN_SENSOR
  result += "W174 Rain Sensor\n" + w174_displayRainSensorParameters() + " \n";
#endif //USE_W174_RAIN_SENSOR
#ifdef USE_WATER_SENSOR
  result += "Water Sensor\n" + displayWaterSensorParameters() + " \n";
#endif //USE_WATER_SENSOR

  setDisplayText(result);
#endif // USE_OLED
}
/**
   Update all sensor data
*/
void updateSensorData() {

  // reset all timers
  sensor_read = { 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned long start = 0;

#ifdef USE_DAVIS_SENSOR
  start = millis();
  readAnemometer();
  sensor_read.davis_read = millis() - start;
#endif //USE_DAVIS_SENSOR

#ifdef USE_RG11_RAIN_SENSOR
  start = millis();
  rg11_updateRainSensor();
  sensor_read.rg11_rainsensor_read = millis() - start;
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
  start = millis();
  w174_updateRainSensor();
  sensor_read.w174_rainsensor_read = millis() - start;
#endif //USE_W174_RAIN_SENSOR

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

#ifdef USE_WATER_SENSOR
  start = millis();
  updatewater();
  sensor_read.water_read = millis() - start;
#endif //USE_WATER_SENSOR

  // set the flag for display text refresh
#ifdef USE_OLED
  oledData.refresh = true;
#endif // USE_OLED
}

/**
   Send current sensor data as JSON document to Serial
*/
String getSensorData(bool pretty) {
  const int docSize = JSON_OBJECT_SIZE(9) + // max 9 sensors
                      JSON_OBJECT_SIZE(1) + // token data
                      JSON_OBJECT_SIZE(4) + // BME280 sensor
                      JSON_OBJECT_SIZE(3) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // MLX90614 sensor
                      JSON_OBJECT_SIZE(3) + // TSL237 sensor
                      JSON_OBJECT_SIZE(7) + // TSL2591 sensor
                      JSON_OBJECT_SIZE(6) + // Davis Anemometer
                      JSON_OBJECT_SIZE(2) + // Water sensor
                      JSON_OBJECT_SIZE(3)*2;  // Rain sensors
  StaticJsonDocument < docSize > weatherDoc;

  unsigned long start = 0;

#ifdef USE_DAVIS_SENSOR
  serializeAnemometer(weatherDoc);
#endif //USE_DAVIS_SENSOR

#ifdef USE_RG11_RAIN_SENSOR
  rg11_serializeRainSensor(weatherDoc);
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
  w174_serializeRainSensor(weatherDoc);
#endif //USE_W174_RAIN_SENSOR

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

#ifdef USE_WATER_SENSOR
  serializewater(weatherDoc);
#endif //USE_WATER_SENSOR

  String result = "";
  if (pretty)
    serializeJsonPretty(weatherDoc, result);
  else
    serializeJson(weatherDoc, result);

  return result;
}


String getCurrentVersion() {
  StaticJsonDocument <JSON_OBJECT_SIZE(1)> doc;
  doc["version"] = WEATHERRADIO_VERSION;

  String result = "";
  serializeJson(doc, result);

  return result;
}

String getReadDurations() {
  StaticJsonDocument <JSON_OBJECT_SIZE(9)> doc;
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
#ifdef USE_WATER_SENSOR
  if (waterData.status)       doc["Water"]             = sensor_read.water_read;
#endif //USE_WATER_SENSOR
#ifdef USE_RG11_RAIN_SENSOR
  if (rg11_rainsensor_status.status) doc["RG11 Rain Sensor"]   = sensor_read.rg11_rainsensor_read;
#endif //USE_RG11_RAIN_SENSOR
#ifdef USE_W174_RAIN_SENSOR
  if (w174_rainsensor_status.status) doc["W174 Rain Sensor"]   = sensor_read.w174_rainsensor_read;
#endif //USE_W174_RAIN_SENSOR

  String result = "";
  serializeJson(doc, result);

  return result;
}


// translate the sensor configurations to a JSON document
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(7) + // max 7 configurations
                      JSON_OBJECT_SIZE(2) + // DHT sensors
                      JSON_OBJECT_SIZE(3) + // Davis Anemometer
                      JSON_OBJECT_SIZE(1) + // Water sensor
                      JSON_OBJECT_SIZE(2) + // Rain Sensor
                      JSON_OBJECT_SIZE(3) + // WiFi parameters
                      JSON_OBJECT_SIZE(1) + // Arduino
                      JSON_OBJECT_SIZE(4) + // OTA
                      JSON_OBJECT_SIZE(5) + // Dew heater
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

#ifdef USE_WATER_SENSOR
  JsonObject waterdata = doc.createNestedObject("Water");
  waterdata["pin"] = WATER_PIN;
#endif

#ifdef USE_RG11_RAIN_SENSOR
  JsonObject rg11_rainsensordata          = doc.createNestedObject("RG11 Rain Sensor");
  rg11_rainsensordata["rain sensor pin"]  = RG11_RAINSENSOR_PIN;
  rg11_rainsensordata["bucket size"]      = RG11_RAINSENSOR_BUCKET_SIZE;
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
  JsonObject w174_rainsensordata          = doc.createNestedObject("W174 Rain Sensor");
  w174_rainsensordata["rain sensor pin"]  = W174_RAINSENSOR_PIN;
  w174_rainsensordata["bucket size"]      = W174_RAINSENSOR_BUCKET_SIZE;
#endif //USE_W174_RAIN_SENSOR

#ifdef USE_WIFI
  JsonObject wifidata = doc.createNestedObject("WiFi");
  wifidata["SSID"] = WiFi.SSID();
  wifidata["connected"] = WiFi.status() == WL_CONNECTED;
  if (WiFi.status() == WL_CONNECTED)
    wifidata["IP"]        = WiFi.localIP().toString();
  else
    wifidata["IP"]        = "";
#endif

#ifdef USE_DEW_HEATER
  serializeDewheater(doc);
#endif

#ifdef USE_OTA
  serializeOTA(doc);
#endif // USE_OTA

  String result = "";
  serializeJson(doc, result);

  if (doc.isNull())
    return "{}";
  else {
    return result;
  }
}

#ifdef USE_OLED
void oledSingleButtonClicked() {
  // reset the turn off timeout
  oledData.lastShowDisplay = millis();
  // get latest data
  oledData.refresh = true;
  // clear the display
  oled.clear();
  // update the display text
  updateDisplayText();
  // turn on the display
  oledShow(true);
}
#endif // USE_OLED

unsigned long lastSensorRead;

void setup() {
  Serial.begin(BAUD_RATE);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

  String init_text = "Weather Radio V ";
  init_text += WEATHERRADIO_VERSION;
  Serial.println(" \n" + init_text);

  // sensors never read
  lastSensorRead = 0;

#ifdef USE_OLED
  // initial text

  initDisplay();
  setDisplayText(init_text);
#endif


#ifdef USE_DAVIS_SENSOR
  initAnemometer();
#endif //USE_DAVIS_SENSOR

#ifdef USE_RG11_RAIN_SENSOR
  rg11_initRainSensor();
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
  w174_initRainSensor();
#endif //USE_W174_RAIN_SENSOR

#ifdef USE_TSL237_SENSOR
  initTSL237();
#endif //USE_TSL237_SENSOR

#ifdef USE_WIFI
  initWiFi();

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
#endif


#ifdef USE_OTA
  initOTA();
#endif // USE_OTA

#ifdef USE_DEW_HEATER
  initDewheater();
#endif // USE_DEW_HEATER

  // initial readout all sensors
  updateSensorData();

#ifdef USE_OLED
  // define handling of clicks
  displayButton.attachClick(oledSingleButtonClicked);
#endif // USE_OLED

  // initially set display text
  updateDisplayText();

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
      initWiFi();
      break;
    case 'd':
      stopWiFi();
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

#ifdef USE_OTA
  otaLoop();
#endif // USE_OTA

#ifdef USE_WIFI
  wifiServerLoop();
#endif

#ifdef USE_OLED
  // refresh the display text if necessary
  if (oledData.refresh) updateDisplayText();
  // update the display
  updateOledDisplay();
#endif

#ifdef USE_TSL237_SENSOR
  updateTSL237();
#endif //USE_TSL237_SENSOR

#ifdef USE_DAVIS_SENSOR
  updateAnemometer();
#endif //USE_DAVIS_SENSOR

#ifdef USE_DEW_HEATER
  updateDewheater();
#endif // USE_DEW_HEATER

#ifdef USE_RG11_RAIN_SENSOR
  rg11_updateRainSensor();
#endif //USE_RG11_RAIN_SENSOR

#ifdef USE_W174_RAIN_SENSOR
  w174_updateRainSensor();
#endif //USE_W174_RAIN_SENSOR

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
