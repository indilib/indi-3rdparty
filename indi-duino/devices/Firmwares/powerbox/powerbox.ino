/*  Arduino based power box with two switches and two PWM controlled dimmers.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Follows the ideas from this tutorial:
    https://randomnerdtutorials.com/esp8266-pwm-arduino-ide/
*/

#include "config.h"

#include "switches.h"
#include "dimmer.h"


/**
   Create Json lines displaying help information.
*/
void showHelp() {
  String init_text = "Arduino Power Box V ";
  init_text += POWERBOX_VERSION + " - Available commands:";
  addJsonLine(init_text, MESSAGE_INFO);
  addJsonLine("h - show this help message", MESSAGE_INFO);
  addJsonLine("c - show the configuration", MESSAGE_INFO);
  addJsonLine("i - show devices status information", MESSAGE_INFO);
  addJsonLine("f?value=<frequency> - change the PWM frequency", MESSAGE_INFO);
  addJsonLine("d?id=<[1|2|>&value=<duty cycle> - change the PWM duty cycle", MESSAGE_INFO);
  addJsonLine("p?id=<[1|2|>&power=[on|off] - turn PWM on or off", MESSAGE_INFO);
  addJsonLine("s?id=<[1|2|>&power=[on|off] - turn switch on or off", MESSAGE_INFO);
}

void setup() {
  Serial.begin(BAUD_RATE);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;

  initDimmer();
  initSwitches();
}



String getStatus() {
  const int docSize = JSON_OBJECT_SIZE(1) + // top level
                      JSON_OBJECT_SIZE(5) + // max 5 sub nodes (freq, 2xPWM, Arduino, switches)
                      JSON_OBJECT_SIZE(1) + // Arduino
                    2*JSON_OBJECT_SIZE(2) + // PWM parameters
                    2*JSON_OBJECT_SIZE(1);  // switches status
  StaticJsonDocument <docSize> root;
  JsonObject doc = root.createNestedObject("status");

  serializeDimmerStatus(doc);
  serializeSwitchStatus(doc);

  String result = "";
  serializeJson(root, result);

  if (root.isNull())
    return "{}";
  else {
    return result;
  }
}


/**
   translate the configuration to a JSON document
*/
String getCurrentConfig() {
  const int docSize = JSON_OBJECT_SIZE(1) + // top level
                      JSON_OBJECT_SIZE(4) + // 4 pins
                      JSON_OBJECT_SIZE(1);  // version
  StaticJsonDocument <docSize> root;
  JsonObject doc = root.createNestedObject("config");

  doc["version"]      = POWERBOX_VERSION;
  doc["PWM pin 1"]    = PWM_PIN_1;
  doc["PWM pin 2"]    = PWM_PIN_2;
  doc["switch pin 1"] = POWER_PIN_1;
  doc["switch pin 2"] = POWER_PIN_2;

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
      parsePWMControl(input);
      addJsonLine(getStatus());
      break;
    case 'f':
      parseFrequency(input);
      addJsonLine(getStatus());
      break;
    case 'd':
      parseDutyCycle(input);
      addJsonLine(getStatus());
      break;
    case 's':
      parseSwitchControl(input);
      addJsonLine(getStatus());
      break;
  }

}

/**
   Command loop handling incoming requests and returns a JSON document.
*/
void loop() {
  byte ch;
  static String input = "";


  if (Serial.available() > 0) {
    ch = Serial.read();

    if (ch == '\r' || ch == '\n') { // Command received and ready.
      parseInput(input);
      Serial.println(processJsonLines());
      input = "";
    }
    else
      input += (char)ch;
  }
}
