/*  PWM based dimmer for ESP8266.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Follows the ideas from this tutorial:
    https://randomnerdtutorials.com/esp8266-pwm-arduino-ide/
*/

#include "dimmer.h"


void setup() {
  Serial.begin(BAUD_RATE);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) continue;
  Serial.println("Initialized");

  setPower(false);
  setFrequency(40000);
  setDutyCycle(0);

#ifdef USE_WIFI
  initWiFi();

  server.on("/", []() {
    addJsonLine(getStatus());
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/h", []() {
    showHelp();
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/p", []() {
    setPower(true);
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/x", []() {
    setPower(false);
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/x", []() {
    setPower(false);
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/f", []() {
    String freq = server.arg("value");
    setFrequency(freq.toInt());
    server.send(200, "application/json; charset=utf-8", processJsonLines());
  });

  server.on("/d", []() {
    String cycle = server.arg("value");
    setDutyCycle(cycle.toInt());
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

  server.on("/i", []() {
    addJsonLine(getStatus());
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

#ifdef USE_WIFI
  wifiServerLoop();
#endif

}
