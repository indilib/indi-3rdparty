/*  Power switches.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

struct {
  // power status (on = true)
  bool switch_1_power = false;
  bool switch_2_power = false;
} power_data;


/**
   Turning a switch on or off
*/
void setSwitchPower(int pin, bool on) {
  digitalWrite(pin, on ? (POWER_INVERTED ? LOW : HIGH) : (POWER_INVERTED ? HIGH : LOW));
  // store value
  switch (pin) {
    case POWER_PIN_1:
      power_data.switch_1_power = on;
      break;
    case POWER_PIN_2:
      power_data.switch_2_power = on;
      break;
  }
}


/**
   Initialize the switches
*/
void initSwitches() {
  pinMode(POWER_PIN_1, OUTPUT);
  pinMode(POWER_PIN_2, OUTPUT);
  setSwitchPower(POWER_PIN_1, false);
  setSwitchPower(POWER_PIN_2, false);
}


/** Parse switch control
    example "id=[1|2]&power=[on|off]" */
void parseSwitchControl(String input) {
  if (input.length() <= 2 || input.charAt(1) != '?')
    return;
  int begin = 2;
  int end = input.indexOf('=', begin);
  int next = 0;

  String name;
  String value;
  int pin ;
  bool status;

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

    if (name == String("id")) pin = value.toInt() == 2 ? POWER_PIN_2 : POWER_PIN_1;
    if (name == String("power"))  status = (value == "on");
  }

  setSwitchPower(pin, status);
}

/**
   translate the dimmer status into a JSON document
*/
void serializeSwitchStatus(JsonObject &doc) {
  JsonObject switchdata_1 = doc.createNestedObject("Switch 1");
  switchdata_1["power"] = power_data.switch_1_power ? "on" : "off";
  JsonObject switchdata_2 = doc.createNestedObject("Switch 2");
  switchdata_2["power"] = power_data.switch_2_power ? "on" : "off";
}
