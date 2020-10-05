/*  Integration of an OLED display

    Copyright (C) 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

SSD1306AsciiWire oled;

struct {
  String oled_display_text;
  unsigned long lastShowDisplay;
  bool show;
} oledData {"", 0, true};


void oledShow (bool status) {
  oledData.show = status;
  // clear display equals turning display off
  oled.ssd1306WriteCmd(status ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

// interrupt called to turn display on
#ifdef ESP8266
void ICACHE_RAM_ATTR isr_oled_show () {
#else
void isr_oled_show () {
#endif
  oledData.lastShowDisplay = millis();
  oledShow(true);
}

void initDisplay() {
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, OLED_I2C_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  oled.setScrollMode(SCROLL_MODE_APP);

  pinMode(OLED_BUTTONPIN, INPUT);
  // attach to react upon interrupts when the reed element closes the circuit
  attachInterrupt(digitalPinToInterrupt(OLED_BUTTONPIN), isr_oled_show, RISING);
}


int oledCountLines(String text) {
  int count = 0;

  for (int i = 0; i < text.length(); i++) {
    if (text.charAt(i) == '\n') count++;
  }
  // check if last line is terminated by \n
  if (text.charAt(text.length() - 1 != '\n'))
    count++;

  return count;
}

char *oled_text = NULL;
char *oled_line = NULL;
uint32_t scrollTime = 0;
bool oled_rolling;
bool oled_print_finished;

void setDisplayText(String text) {

  // Set cursor to last line of memory window.
  oled.setCursor(0, oled.displayRows() - oled.fontRows());

  // copy the text to the buffer and initialize from string
  oledData.oled_display_text = text;
  // determine whether we need to roll the display
  oled_rolling = (oledCountLines(oledData.oled_display_text) > oled.fontRows());
  // fill the first line
  oled_print_finished = false;
  oled_text = NULL;
  oled_line = NULL;
}

void displayText() {
  // check whether the display should be turned off
  if (OLED_DISPLAY_TIMEOUT >= 0 && millis() > oledData.lastShowDisplay + OLED_DISPLAY_TIMEOUT * 1000) {
    oledShow(false);
  }

  // smooth scrolling if display is behind display buffer
  if (!oled.scrollIsSynced()) {
    uint32_t now = millis();
    if ((now - scrollTime) >= OLED_SCROLL_TIMEOUT) {
      // Scroll display window.
      oled.scrollDisplay(1);
      scrollTime = now;
    }
  } else if ((millis() - scrollTime) > OLED_SCROLL_TIMEOUT) {
    if (oled_line == NULL && ! oled_print_finished) {
      // initialize the text to restart the loop
      oled_text = new char[oledData.oled_display_text.length() + 1];
      oledData.oled_display_text.toCharArray(oled_text, oledData.oled_display_text.length() + 1);
      oled_line = strtok(oled_text, "\r\n");
      // do not repeat this if display is not rolling
      if (! oled_rolling)
        oled_print_finished = true;
    } else {
      // take next line
      oled_line = strtok(NULL, "\r\n");
    }
    if (oled_line != NULL) {
      // Scroll memory window.
      oled.scrollMemory(oled.fontRows());
      // jump to line start
      oled.setCol(0);
      // print line and clean up behind
      oled.print(oled_line);
      oled.clearToEOL();
    }
  }
}
