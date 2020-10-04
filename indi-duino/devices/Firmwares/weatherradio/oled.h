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
String oled_display_text;

void initDisplay() {
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, OLED_I2C_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  oled.setScrollMode(SCROLL_MODE_APP);
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
  oled_display_text = text;
  // determine whether we need to roll the display
  oled_rolling = (oledCountLines(oled_display_text) > oled.fontRows());
  // fill the first line
  oled_print_finished = false;
  oled_text = NULL;
  oled_line = NULL;
}

void displayText() {
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
      oled_text = new char[oled_display_text.length() + 1];
      oled_display_text.toCharArray(oled_text, oled_display_text.length() + 1);
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
