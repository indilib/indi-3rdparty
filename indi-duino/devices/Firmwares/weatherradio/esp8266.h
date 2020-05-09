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
#include <uri/UriRegex.h>
#include "memory.h"

#define WIFI_TIMEOUT 20              // try 20 secs to connect until giving up

struct {
  bool status;
  unsigned long lastRetry; // Last time the frequency had been measured
  String ssid;
  String password;
} esp8266Data {false, 0, WIFI_SSID, WIFI_PWD};

ESP8266WebServer server(80);

void reset() {
  ESP.restart();
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(esp8266Data.ssid.c_str(), esp8266Data.password.c_str());

  int count = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && count++ < WIFI_TIMEOUT) {
    esp8266Data.lastRetry = millis();
    delay(1000);
    if (WiFi.status() == WL_CONNECTED) esp8266Data.status = true;
  }
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    esp8266Data.status = (WiFi.status() == WL_CONNECTED);

    int count = 0;
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED && count++ < WIFI_TIMEOUT) {
      esp8266Data.lastRetry = millis();
      delay(1000);
      esp8266Data.status = (WiFi.status() == WL_CONNECTED);
    }
  }
}


void wifiServerLoop() {
  // retry a connect
  if (esp8266Data.status == false && WiFi.status() != WL_CONNECTED) {
    volatile unsigned long now = millis();
    if (now - esp8266Data.lastRetry > 1000)
    {
      WiFi.begin(esp8266Data.ssid, esp8266Data.password);
      esp8266Data.lastRetry = now;
      if (WiFi.status() == WL_CONNECTED) esp8266Data.status = true;
    }
  }

  // handle requests to the WiFi server
  server.handleClient();
}

// Parse ssid and passphrase from input
// example input =
void parseCredentials(String input) {
  int begin = 0;
  int end = input.indexOf('=', begin);
  int next = 0;

  String name;
  String value;

  while (end > begin) {
    name = input.substring(begin, end);
    next = input.indexOf('&', end + 1);

    if (next == -1) {
      // last parameter
      value = input.substring(end + 1);

      // finish
      end = -1;
    } else {
      value = input.substring(end + 1, next);

      // next cycle
      begin = next + 1;
      end = input.indexOf('=', begin);
    }

    if (name == String("ssid"))     esp8266Data.ssid = value;
    if (name == String("password")) esp8266Data.password = value;
  }
}

#endif
