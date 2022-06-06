/*  PWM based dimmer for ESP8266.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

// frequency of the pwm signal
unsigned long pwm_frequency = PWM_FREQ_DEFAULT;

struct {
  // percentage the signal is on (0..255)
  unsigned int  pwm_duty_cycle = PWM_DUTY_CYCLE_DEFAULT;
  // power status (on = true)
  bool pwm_power = false;
} pwm_data_1, pwm_data_2;

/**
   translate the dimmer status into a JSON document
*/
void serializeDimmerStatus(JsonObject &doc) {
  doc["PWM frequency"] = pwm_frequency;
  JsonObject pwmdata_1 = doc.createNestedObject("PWM 1");
  pwmdata_1["power"]      = pwm_data_1.pwm_power ? "on" : "off";
  pwmdata_1["duty cycle"] = pwm_data_1.pwm_duty_cycle;
  JsonObject pwmdata_2 = doc.createNestedObject("PWM 2");
  pwmdata_2["power"]      = pwm_data_2.pwm_power ? "on" : "off";
  pwmdata_2["duty cycle"] = pwm_data_2.pwm_duty_cycle;
}

/**
   Turn PWM on or off
*/
void setPower(int pin, bool on) {

  switch (pin) {
    case PWM_PIN_1:
      pwm_data_1.pwm_power = on;
      analogWrite(pin, on ? pwm_data_1.pwm_duty_cycle : 0);
      break;
    case PWM_PIN_2:
      pwm_data_2.pwm_power = on;
      analogWrite(pin, on ? pwm_data_2.pwm_duty_cycle : 0);
      break;
  }
}


/** Parse PWM power control
    example "p?id=[1|2]&power=[on|off]" */
void parsePWMControl(String input) {
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

    if (name == String("id")) pin = value.toInt() == 1 ? PWM_PIN_1 : PWM_PIN_2;
    if (name == String("power"))  status = (value == "on");
  }
  setPower(pin, status);
}

/**
   Set the PWM duty cycle
*/
void setDutyCycle(int pin, long value) {
  switch (pin) {
    case PWM_PIN_1:
      pwm_data_1.pwm_duty_cycle = value % 256;
      if (pwm_data_1.pwm_power) analogWrite(PWM_PIN_1, pwm_data_1.pwm_duty_cycle);
      break;
    case PWM_PIN_2:
      pwm_data_2.pwm_duty_cycle = value % 256;
      if (pwm_data_2.pwm_power) analogWrite(PWM_PIN_2, pwm_data_2.pwm_duty_cycle);
      break;
  }
}

/** Parse PWM duty cycle
    example "d?id=[1|2]&cycle=[on|off]" */
void parseDutyCycle(String input) {
  if (input.length() <= 2 || input.charAt(1) != '?')
    return;
  int begin = 2;
  int end = input.indexOf('=', begin);
  int next = 0;

  String name;
  String value;
  int pin = PWM_PIN_1, cycle = 0;

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

    if (name == String("id"))    pin = value.toInt() == 1 ? PWM_PIN_1 : PWM_PIN_2;
    if (name == String("value")) cycle = value.toInt();

  }
  setDutyCycle(pin, cycle);
}

/**
   Set the PWM frequency
*/
void setFrequency(long value) {
  if (value > 0) pwm_frequency = value;
  // change the frequency
  analogWriteFreq(pwm_frequency);
}

void parseFrequency(String input) {
  if (input.length() <= 2 || input.charAt(1) != '?')
    return;
  int begin = 2;
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

    // handle value
    if (name == String("value")) setFrequency(value.toInt());
  }
}

/**
   Initialize the dimmer
*/
void initDimmer() {
  pinMode(PWM_PIN_1, OUTPUT);
  pinMode(PWM_PIN_2, OUTPUT);
  setPower(PWM_PIN_1, false);
  setPower(PWM_PIN_2, false);
  setFrequency(PWM_FREQ_DEFAULT);
  setDutyCycle(PWM_PIN_1, 0);
  setDutyCycle(PWM_PIN_2, 0);
}
