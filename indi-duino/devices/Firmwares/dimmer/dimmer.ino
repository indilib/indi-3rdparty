/*  PWM based dimmer for ESP8266.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "dimmer.h"


void setup() {
  Serial.begin(BAUD_RATE);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;
  Serial.println("Initialized");

  // set PWM frequency
  // analogWriteFreq(pwm_data.pwm_frequency);
  // set duty cycle
  // analogWrite(PWM_PIN, pwm_data.pwm_duty_cycle);

#ifdef USE_WIFI
  initWiFi();

  server.on("/h", []() {
    showHelp();
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/r", []() {
    reset();
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/c", []() {
    addJsonLine(getCurrentConfig());
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Ressource not found: " + server.uri());
  });

  server.begin();
#endif
}


/**
   Command loop handling incoming requests and returns a JSON document.
*/
void loop() {
  byte ch;

  if (Serial.available() > 0) {
    ch = Serial.read();

    if (ch == '\r' || ch == '\n') { // Command received and ready.
      parseInput();
      Serial.println(processJsonLines());
      input = "";
    }
    else
      input += (char)ch;
  }

}
