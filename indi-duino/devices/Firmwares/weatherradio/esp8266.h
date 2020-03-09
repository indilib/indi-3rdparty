/*  Web server functionality for ESP8266.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#ifndef STASSID
#define STASSID "your WiFi SSID"
#define STAPSK  "your WiFi password"
#define WIFI_TIMEOUT 20              // try 20 secs to connect until giving up
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int count = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && count++ < WIFI_TIMEOUT) {
    delay(1000);
  }
}

void loop8266() {
  server.handleClient();
}
