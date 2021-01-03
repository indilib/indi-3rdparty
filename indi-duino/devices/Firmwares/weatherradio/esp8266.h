/*  Web server functionality for ESP8266.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

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

#define WIFI_MAX_RECONNECT      10  // try 10 times to connect until giving up
#define WIFI_SLEEP_CONNECT    1000  // wait 1 second until next connection retry
#define WIFI_SLEEP_RECONNECT 60000  // wait 1 minute before retry to connect

enum wifi_status {
  WIFI_IDLE,              // wifi is not connected
  WIFI_CONNECTING,       // start to connect to wifi access point
  WIFI_DISCONNECTING,    // start to disconnect to wifi access point
  WIFI_CONNECTED,        // connection to wifi access point established
  WIFI_CONNECTION_FAILED // connection to wifi access point failed
};

struct {
  wifi_status status;       // wifi connection status
  int retry_count;          // retry counter
  unsigned long last_retry; // Last time connecting to the access point has been tried
  String ssid;              // access point id
  String password;          // access point password
} esp8266Data {WIFI_IDLE, 0, 0, WIFI_SSID, WIFI_PWD};

ESP8266WebServer server(80);

void reset() {
  ESP.restart();
}

void refreshDisplay() {
  // set the flag for display text refresh
#ifdef USE_OLED
  oledData.refresh = true;
#endif // USE_OLED
}

// turn wifi on and connect to the access point
void initWiFi() {
  // set wifi to station mode
  WiFi.mode(WIFI_STA);
  // start trying to connect
  esp8266Data.status = WIFI_CONNECTING;
  Serial.print("Connecting WiFi ");
  // connection attempts inside of wifiServerLoop
}


// try to connect to WiFi
void connectWiFi() {
  esp8266Data.last_retry = millis();
  if (WiFi.status() == WL_CONNECTED) {
    esp8266Data.status = WIFI_CONNECTED;
    esp8266Data.retry_count = 0;
    refreshDisplay();
    Serial.println(" succeeded.");
  } else {
    WiFi.begin(esp8266Data.ssid, esp8266Data.password);
    if (WiFi.status() == WL_CONNECTED) {
      esp8266Data.status = WIFI_CONNECTED;
      // reset retry counter
      esp8266Data.retry_count = 0;
      refreshDisplay();
      Serial.println(" succeeded.");
    } else {
      // increase retry counter
      esp8266Data.retry_count++;
      // check if reconnect limit has been reached
      if (esp8266Data.retry_count <= WIFI_MAX_RECONNECT) {
        esp8266Data.status = WIFI_CONNECTING;
        Serial.print(".");
      } else {
        esp8266Data.status =  WIFI_CONNECTION_FAILED;
        refreshDisplay();
        Serial.println(" failed!");
      }
    }
  }
}


void stopWiFi() {
  esp8266Data.retry_count = 0;
  esp8266Data.status = WIFI_DISCONNECTING;
  Serial.print("Disconnecting WiFi ");
}

void disconnectWiFi() {
  esp8266Data.last_retry = millis();
  if (WiFi.status() == WL_CONNECTED) WiFi.disconnect();

  if (WiFi.status() == WL_CONNECTED) {
    // increase retry counter
    esp8266Data.retry_count++;
    // check if reconnect limit has been reached
    if (esp8266Data.retry_count <= WIFI_MAX_RECONNECT) {
      esp8266Data.status = WIFI_DISCONNECTING;
      Serial.print(".");
    } else {
      esp8266Data.status =  WIFI_CONNECTED;
      refreshDisplay();
      Serial.println("failed!");
    }
  } else {
    esp8266Data.status = WIFI_IDLE;
    // reset retry counter
    esp8266Data.retry_count = 0;
    refreshDisplay();
    Serial.println("succeeded.");
  }
}

void wifiServerLoop() {
  // act depending upon the current connection status
  switch (esp8266Data.status) {
    case WIFI_IDLE:
      // do nothing
      break;

    case WIFI_CONNECTING:
      // retry if connect delay has passed
      if (millis() - esp8266Data.last_retry > WIFI_SLEEP_CONNECT) connectWiFi();
      break;

    case WIFI_DISCONNECTING:
      // retry if disconnect delay has passed
      if (millis() - esp8266Data.last_retry > WIFI_SLEEP_CONNECT) disconnectWiFi();
      break;

    case WIFI_CONNECTED:
      // check if connection is still valid
      if (WiFi.status() != WL_CONNECTED) {
        esp8266Data.status = WIFI_CONNECTING;
        esp8266Data.retry_count = 0;
        Serial.print("WiFi lost, reconnecting ");
        connectWiFi();
      }
      break;

    case WIFI_CONNECTION_FAILED:
      // retry if the reconnect delay has passed
      if (millis() - esp8266Data.last_retry > WIFI_SLEEP_RECONNECT) {
        esp8266Data.status = WIFI_CONNECTING;
        esp8266Data.retry_count = 0;
        Serial.print("Retry connecting WiFi ");
        connectWiFi();
      }
      break;
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

String displayWiFiParameters() {
  String result = "WiFi: " + WiFi.SSID();
  if (WiFi.status() == WL_CONNECTED) {
    result += "\n IP: " + WiFi.localIP().toString() + "\n";
  } else {
    result += "\n status: disconnected\n";
  }

  return result;
}

#endif
