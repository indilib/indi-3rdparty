/*  Arduino over the air updating

    Copyright (C) 2019 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

struct {
  bool init;
  unsigned int progress;
  unsigned int total;
  char* error;
} otaData {false, 0, 0, (char *)""};

void initOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    // Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    // Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    otaData.progress = progress;
    otaData.total = total;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    switch (error) {
      case OTA_AUTH_ERROR:
        otaData.error = (char *)"auth failed";
        break;
      case OTA_BEGIN_ERROR:
        otaData.error = (char *)"begin failed";
        break;
      case OTA_CONNECT_ERROR:
        otaData.error = (char *)"connect failed";
        break;
      case OTA_RECEIVE_ERROR:
        otaData.error = (char *)"receive failed";
        break;
      case OTA_END_ERROR:
        otaData.error = (char *)"end failed";
        break;
    }
  });
}

void serializeOTA(JsonObject &doc) {

  JsonObject data = doc.createNestedObject("OTA");
  data["init"] = otaData.init;

  if (otaData.init) {
    data["progress"] = otaData.progress;
    data["total"] = otaData.total;
    data["error"] = otaData.error;
  }
}

void otaLoop() {
  if (otaData.init == false && WiFi.status() == WL_CONNECTED) {
    // lazy instantiation to ensure that OTA is started AFTER WiFi is connected
    ArduinoOTA.begin();
    otaData.init = true;
  } else {
    ArduinoOTA.handle();
  }
}
