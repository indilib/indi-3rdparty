/*  Web server functionality for ESP8266.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#define WIFI_TIMEOUT 20              // try 20 secs to connect until giving up

const char* ssid = WIFI_SSID;
const char* password = WIFI_PWD;

struct {
  bool status;
  unsigned long lastRetry; // Last time the frequency had been measured
} esp8266Data {false, 0};

ESP8266WebServer server(80);

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int count = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && count++ < WIFI_TIMEOUT) {
    esp8266Data.lastRetry = millis();
    delay(1000);
    if (WiFi.status() == WL_CONNECTED) esp8266Data.status = true;
  }
}

void wifiServerLoop() {
  // retry a connect
  if (esp8266Data.status == false && WiFi.status() != WL_CONNECTED) {
    volatile unsigned long now = millis();
    if (now - esp8266Data.lastRetry > 1000)
    {
      WiFi.begin();
      esp8266Data.lastRetry = now;
      if (WiFi.status() == WL_CONNECTED) esp8266Data.status = true;
    }
  }

  // handle requests to the WiFi server
  server.handleClient();
}
#endif
