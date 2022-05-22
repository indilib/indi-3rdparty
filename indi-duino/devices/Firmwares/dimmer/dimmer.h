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
  // power status (on = true)
  bool pwm_power = false;
} pwm_data;
/**
   Create Json lines displaying help information.
*/
void showHelp() {
  String init_text = "Dimmer V ";
  init_text += DIMMER_VERSION + " - Available commands:";
  addJsonLine(init_text, MESSAGE_INFO);
  addJsonLine("h - show this help message", MESSAGE_INFO);
  addJsonLine("i - show PWM info", MESSAGE_INFO);
  addJsonLine("p - turn PWM on", MESSAGE_INFO);
  addJsonLine("x - turn PWM ff", MESSAGE_INFO);
  addJsonLine("f=<frequency> - change the PWM frequency", MESSAGE_INFO);
  addJsonLine("d=<duty cycle> - change the PWM duty cycle", MESSAGE_INFO);
#ifdef USE_WIFI
  addJsonLine("s?ssid=<wifi ssid>&password=<wifi password> - connect to WiFi access point", MESSAGE_INFO);
  addJsonLine("r - reconnect WiFi", MESSAGE_INFO);
#endif
}


/**
   translate the dimmer status into a JSON document
*/
void serializeDimmerStatus(JsonObject &doc) {
  JsonObject pwmdata = doc.createNestedObject("PWM");
  pwmdata["power on"]   = pwm_data.pwm_power;
  pwmdata["frequency"]  = pwm_data.pwm_frequency;
  pwmdata["duty cycle"] = pwm_data.pwm_duty_cycle;
}

String getStatus() {
  const int docSize = JSON_OBJECT_SIZE(1) + // top level
                      JSON_OBJECT_SIZE(1) + // 1 sub node
                      JSON_OBJECT_SIZE(3);  // PWM parameters
  StaticJsonDocument <docSize> root;
  JsonObject doc = root.createNestedObject("status");

  serializeDimmerStatus(doc);

  String result = "";
  serializeJson(root, result);

  if (root.isNull())
    return "{}";
  else {
    return result;
  }
}

/**
   Turn PWM on or off
*/
void setPower(bool on) {
  // ignore identical values
  if (on == pwm_data.pwm_power)
    return;
    
  pwm_data.pwm_power = on;
  if (on) analogWrite(PWM_PIN, pwm_data.pwm_duty_cycle);
  else    analogWrite(PWM_PIN, 0);
  addJsonLine(getStatus());
}

/**
   Set the PWM duty cycle
*/
void setDutyCycle(long value) {
  pwm_data.pwm_duty_cycle = value % 256;
  // change the duty cycle if power is on
  if (pwm_data.pwm_power)
  {
    analogWrite(PWM_PIN, pwm_data.pwm_duty_cycle);
    // report new status
    addJsonLine(getStatus());
  }
}

void parseDutyCycle(String input) {
  // ignore invalid input
  if (input.length() <= 2 || input.charAt(1) != '=')
    return;
  // handle value
  setDutyCycle(input.substring(2).toInt());
}

/**
   Set the PWM frequency
*/
void setFrequency(long value) {
  if (value > 0) pwm_data.pwm_frequency = value;
  // change the frequency
  analogWriteFreq(pwm_data.pwm_frequency);
  // report new status
  addJsonLine(getStatus());
}

void parseFrequency(String input) {
  // ignore invalid input
  if (input.length() <= 2 || input.charAt(1) != '=')
    return;
  // handle value
  setFrequency(input.substring(2).toInt());
}

/**
   translate the configuration to a JSON document
*/
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(1) + // top level
                      JSON_OBJECT_SIZE(7) + // max 7 configurations
                      JSON_OBJECT_SIZE(1) + // Arduino
                      JSON_OBJECT_SIZE(6) + // WiFi parameters
                      JSON_OBJECT_SIZE(3) + // PWM parameters
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

  serializeDimmerStatus(doc);

  String result = "";
  serializeJson(root, result);

  if (root.isNull())
    return "{}";
  else {
    return result;
  }
}



/**
   Parse the input read from the serial line and translate
*/
// it into commands
void  parseInput(String input) {
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
    case 'i':
      addJsonLine(getStatus());
      break;
    case 'p':
      setPower(true);
      break;
    case 'x':
      setPower(false);
      break;
    case 'f':
      parseFrequency(input);
      break;
    case 'd':
      parseDutyCycle(input);
      break;
#ifdef USE_WIFI
    case 's':
      parseCredentials(input);
      initWiFi();
      break;
    case 'r':
      reset();
      break;
    case 'o':
      stopWiFi();
      break;
#endif
  }
}
