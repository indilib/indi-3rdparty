/*  PWM based dimmer for ESP8266.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "config.h"

struct {
  // frequency of the pwm signal
  unsigned long pwm_frequency = PWM_FREQ_DEFAULT;
  // percentage the signal is on (0..255)
  unsigned int  pwm_duty_cycle = PWM_DUTY_CYCLE_DEFAULT;
} pwm_data;
/**
   Create Json lines displaying help information.
*/
void showHelp() {
  String init_text = "Dimmer V ";
  init_text += DIMMER_VERSION;
  addJsonLine(init_text, MESSAGE_INFO);
  addJsonLine("Available commands:", MESSAGE_INFO);
  addJsonLine("h - show this help message", MESSAGE_INFO);
#ifdef USE_WIFI
  addJsonLine("r - reconnect WiFi", MESSAGE_INFO);
  addJsonLine("s?ssid=<wifi ssid>&password=<wifi password> - connect to WiFi access point", MESSAGE_INFO);
#endif
}


/**
   translate the configuration to a JSON document
*/
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(1) + // top level
                      JSON_OBJECT_SIZE(7) + // max 7 configurations
                      JSON_OBJECT_SIZE(1) + // Arduino
                      JSON_OBJECT_SIZE(6) + // WiFi parameters
                      JSON_OBJECT_SIZE(2) + // PWM parameters
                      JSON_OBJECT_SIZE(2);  // buffer
  StaticJsonDocument <docSize> root;
  JsonObject doc = root.createNestedObject("config");

#ifdef USE_WIFI
  // currently, we have memory info only available for ESP8266
  JsonObject arduinodata = doc.createNestedObject("Arduino");
  arduinodata["free memory"] = freeMemory();

  JsonObject wifidata = doc.createNestedObject("WiFi");
  wifidata["SSID"]      = WiFi.SSID();
  switch (getWiFiStatus()) {
    case WIFI_CONNECTED:
      wifidata["status"]    = "connected";
      wifidata["IP"]        = WiFi.localIP();
      wifidata["rssi"]      = WiFi.RSSI();
      wifidata["ping (ms)"] = networkData.avg_response_time;
      wifidata["loss"]      = networkData.loss;
      break;
    case WIFI_IDLE:
      wifidata["status"]    = "disconnected";
      break;
    case WIFI_CONNECTING:
      wifidata["status"]    = "connecting";
      wifidata["retry"]     = esp8266Data.retry_count;
      break;
    case WIFI_DISCONNECTING:
      wifidata["status"]    = "disconnecting";
      wifidata["retry"]     = esp8266Data.retry_count;
      break;
    case WIFI_CONNECTION_FAILED:
      wifidata["status"]    = "connection failed";
      break;
  }
#endif

  JsonObject pwmdata = doc.createNestedObject("PWM");
  pwmdata["frequency"] = pwm_data.pwm_frequency;
  pwmdata["duty cycle"] = pwm_data.pwm_duty_cycle;

  String result = "";
  serializeJson(root, result);

  if (root.isNull())
    return "{}";
  else {
    return result;
  }
}

String input = "";

/**
   Parse the input read from the serial line and translate
*/
// it into commands
void  parseInput() {
  // ignore empty input
  if (input.length() == 0)
    return;

  switch (input.charAt(0)) {
    case 'h':
      showHelp();
      break;
    case 'c':
      addJsonLine(getCurrentConfig());
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
