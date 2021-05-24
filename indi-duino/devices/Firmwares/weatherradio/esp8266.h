/*  Web server functionality for ESP8266.

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifdef ESP8266
#include <Pinger.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>
#include "memory.h"

#define WIFI_MAX_RECONNECT      10  // try 10 times to connect until giving up
#define WIFI_SLEEP_CONNECT    5000  // wait 1 second until next connection retry
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

Pinger pinger;
struct {
  String dest_ip_address;
  u32_t loss;
  u32_t avg_response_time;
  unsigned long last_retry;
} networkData {"", 0, 0, 0};

void reset() {
  ESP.restart();
  addJsonLine("Arduino restarted successfully", MESSAGE_INFO);
}

void refreshDisplay() {
  // set the flag for display text refresh
#ifdef USE_OLED
  oledData.refresh = true;
#endif // USE_OLED
}

// Retrieve the current status. It has the following side effects
// WIFI_CONNECTING --> WIFI_CONNECTED
// WIFI_CONNECTED --> WIFI_CONNECTING
// WIFI_DISCONNECTING --> WIFI_IDLE
wifi_status getWiFiStatus() {
  wl_status_t status = WiFi.status();
  switch (status) {

    case WL_CONNECTED:
      if (esp8266Data.status == WIFI_CONNECTING) {
        if (esp8266Data.retry_count > 0)
          addJsonLine("Connecting WiFi ... (succeeded, retry=" + String(esp8266Data.retry_count) + ")", MESSAGE_INFO);
        else
          addJsonLine("Connecting WiFi ... (succeeded)", MESSAGE_INFO);
        esp8266Data.status = WIFI_CONNECTED;
      }
      break;

    case WL_IDLE_STATUS:
    case WL_CONNECTION_LOST:
    case WL_DISCONNECTED:
      switch (esp8266Data.status) {
        case WIFI_CONNECTED:
          addJsonLine("WiFi disconnected, reconnecting...", MESSAGE_INFO);
          esp8266Data.status = WIFI_CONNECTING;
          break;
        case WIFI_DISCONNECTING:
          addJsonLine("Disconnecting WiFi ... (succeeded)", MESSAGE_INFO);
          esp8266Data.status = WIFI_IDLE;
        default: // do nothing
          break;
      }
      break;

    case WL_CONNECT_FAILED:
      switch (esp8266Data.status) {
        case WIFI_CONNECTED:
        case WIFI_CONNECTING:
          addJsonLine("WiFi connection failed.", MESSAGE_INFO);
          esp8266Data.status = WIFI_CONNECTION_FAILED;
          break;
        case WIFI_DISCONNECTING:
          addJsonLine("Disconnecting WiFi ... (succeeded)", MESSAGE_INFO);
          esp8266Data.status = WIFI_IDLE;
        default: // do nothing
          break;
      }
      break;
    case WL_WRONG_PASSWORD:
      if (esp8266Data.status == WIFI_CONNECTING) {
        addJsonLine("WiFi connection failed, wrong password.", MESSAGE_INFO);
        esp8266Data.status = WIFI_CONNECTION_FAILED;
      }
      break;

    case WL_NO_SHIELD:
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED:
    default:
      break;
  }
  return esp8266Data.status;
}

// turn wifi on and connect to the access point
void initWiFi() {
  // set wifi to station mode
  WiFi.mode(WIFI_STA);
  // start trying to connect
  esp8266Data.status = WIFI_CONNECTING;
  esp8266Data.retry_count = 0;
  esp8266Data.last_retry = millis();

  addJsonLine("Connecting WiFi ...", MESSAGE_INFO);

  pinger.OnEnd([](const PingerResponse & response)
  {
    networkData.dest_ip_address   = response.DestIPAddress.toString();
    networkData.avg_response_time = response.AvgResponseTime;
    networkData.loss              = response.TotalSentRequests - response.TotalReceivedResponses;
    networkData.last_retry        = millis();

    addJsonLine("Ping " + networkData.dest_ip_address + ", avg time=" + String(networkData.avg_response_time) + " ms, loss=" +
                String(networkData.loss), MESSAGE_DEBUG);
    return true;
  });
}


// try to connect to WiFi
void connectWiFi() {
  esp8266Data.last_retry = millis();
  wifi_status status = getWiFiStatus();
  switch (status) {
    case WIFI_CONNECTED:
      addJsonLine("WiFi already connected.", MESSAGE_DEBUG);
      esp8266Data.retry_count = 0;
      refreshDisplay();
      break;
    case WIFI_DISCONNECTING:
      addJsonLine("Disconnect stopped, connecting...", MESSAGE_INFO);
    // fallthrough
    default:
      // check if reconnect limit has been reached
      if (esp8266Data.retry_count < WIFI_MAX_RECONNECT) {
        esp8266Data.status = WIFI_CONNECTING;
        // increase retry counter
        esp8266Data.retry_count++;
        // try to connect
        addJsonLine("WiFi.begin(..., ...)", MESSAGE_DEBUG);
        WiFi.begin(esp8266Data.ssid, esp8266Data.password);
      } else {
        esp8266Data.status =  WIFI_CONNECTION_FAILED;
        esp8266Data.retry_count = 0;
        refreshDisplay();
        addJsonLine("Connecting WiFi ... FAILED!", MESSAGE_WARN);
      }
      break;
  }
}

void disconnectWiFi() {
  esp8266Data.last_retry = millis();
  wifi_status status = getWiFiStatus();
  switch (status) {
    case WIFI_CONNECTING:
      addJsonLine("Connecting stopped, disconnecting...", MESSAGE_INFO);
    // fallthrough
    case WIFI_CONNECTION_FAILED:
      esp8266Data.status = WIFI_DISCONNECTING;
    // fallthrough
    case WIFI_CONNECTED:
      esp8266Data.retry_count = 0;
      WiFi.disconnect();
      break;
    case WIFI_DISCONNECTING:
      // check if reconnect limit has been reached
      if (esp8266Data.retry_count <= WIFI_MAX_RECONNECT) {
        // increase retry counter
        esp8266Data.retry_count++;
        WiFi.disconnect();
      } else {
        esp8266Data.status =  WIFI_CONNECTED;
        refreshDisplay();
        addJsonLine("Disconnecting WiFi ... FAILED!", MESSAGE_WARN);
      }
      break;
    case WIFI_IDLE:
      esp8266Data.status = WIFI_IDLE;
      // reset retry counter
      esp8266Data.retry_count = 0;
      refreshDisplay();
      addJsonLine("Disconnecting WiFi ... (succeeded)", MESSAGE_INFO);
      break;
  }
}

// initialize stopping the WiFi
void stopWiFi() {
  esp8266Data.retry_count = 0;
  esp8266Data.last_retry  = millis();
  esp8266Data.status = WIFI_DISCONNECTING;
  addJsonLine("Disconnecting WiFi ...", MESSAGE_INFO);
}

void wifiServerLoop() {
  uint32_t now = millis();


  // act depending upon the current connection status
  switch (getWiFiStatus()) {
    case WIFI_IDLE:
      // do nothing
      break;

    case WIFI_CONNECTING:
      // retry if connect delay has passed
      if (now - esp8266Data.last_retry > WIFI_SLEEP_CONNECT) connectWiFi();
      break;

    case WIFI_DISCONNECTING:
      // retry if disconnect delay has passed
      if (now - esp8266Data.last_retry > WIFI_SLEEP_CONNECT) disconnectWiFi();
      break;

    case WIFI_CONNECTED:
      // check if gateway is reachable
      if (now - networkData.last_retry > WIFI_SLEEP_RECONNECT) {
        if (pinger.Ping(WiFi.gatewayIP(), 4) == false || networkData.loss > 3) {
          addJsonLine("Cannot reach gateway, try to reconnect WiFi ...", MESSAGE_WARN);
          esp8266Data.retry_count = 0;
          connectWiFi();
        }
        networkData.last_retry = now;
      }
      break;

    case WIFI_CONNECTION_FAILED:
      // retry if the reconnect delay has passed
      if (millis() - esp8266Data.last_retry > WIFI_SLEEP_RECONNECT) {
        esp8266Data.status = WIFI_CONNECTING;
        esp8266Data.retry_count = 0;
        addJsonLine("Retry connecting WiFi ...", MESSAGE_INFO);
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

    if (name == String("ssid"))     esp8266Data.ssid     = value;
    if (name == String("password")) esp8266Data.password = value;
  }
}

String displayWiFiParameters() {

  String result = "WiFi: " + WiFi.SSID();
  switch (getWiFiStatus()) {
    case WIFI_CONNECTED:
      result += "\n IP: " + WiFi.localIP().toString() + "\n";
      break;
    case WIFI_IDLE:
      result += "\n status: disconnected\n";
      break;
    case WIFI_CONNECTING:
      result += "\n status: connecting\n";
      result += "\n retry: " + String(esp8266Data.retry_count) + "\n";
      break;
    case WIFI_DISCONNECTING:
      result += "\n status: disconnecting\n";
      result += "\n retry: " + String(esp8266Data.retry_count) + "\n";
      break;
    case WIFI_CONNECTION_FAILED:
      result += "\n status: conn. failed\n";
      break;
  }
  return result;
}

#endif
