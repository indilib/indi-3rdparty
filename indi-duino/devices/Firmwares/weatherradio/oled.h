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
#include "OneButton.h"

SSD1306AsciiWire oled;

struct {
  char * text;
  char * line;
  char * text_orig;
  unsigned long lastShowDisplay;
  bool show;
  bool refresh;
} oledData {NULL, NULL, NULL, 0, true, true};

OneButton displayButton;


void oledShow (bool status) {
  oledData.show = status;
  // clear display equals turning display off
  oled.ssd1306WriteCmd(status ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
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

uint32_t scrollTime = 0;
bool oled_rolling;
bool oled_print_finished;

void setDisplayText(String text) {

  // Set cursor to last line of memory window.
  oled.setCursor(0, oled.displayRows() - oled.fontRows());

  // clean up
  if (oledData.text != NULL) free(oledData.text);
  if (oledData.text_orig != NULL) free(oledData.text_orig);
  oledData.text = NULL;
  oledData.line = NULL;

  // copy the text to the buffer and initialize from string
  oledData.text = new char[text.length() + 1];
  oledData.text_orig = new char[text.length() + 1];
  text.toCharArray(oledData.text, text.length() + 1);
  strcpy(oledData.text_orig, oledData.text);

  // determine whether we need to roll the display
  oled_rolling = (oledCountLines(text) > oled.fontRows());
  // fill the first line
  oled_print_finished = false;

  // clear the refresh flag
  oledData.refresh = false;
}


void initDisplay() {
  Wire.begin();
  Wire.setClock(OLED_WIRE_CLOCK_SPEED);
  oled.begin(&Adafruit128x32, OLED_I2C_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  oled.setScrollMode(SCROLL_MODE_APP);

  // initialize the display button
  displayButton = OneButton(OLED_BUTTONPIN,
                            false,       // Button is active high
                            false        // Disable internal pull-up resistor
                           );

}

void updateOledDisplay() {
  // update button state
  displayButton.tick();
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
    if (oledData.line == NULL && ! oled_print_finished) {
      // initialize the text to restart the loop
      strcpy(oledData.text, oledData.text_orig);
      oledData.line = strtok(oledData.text, "\r\n");

      // do not repeat this if display is not rolling
      if (! oled_rolling) oled_print_finished = true;
    } else {
      // take next line
      oledData.line = strtok(NULL, "\r\n");
    }
    if (oledData.line != NULL) {
      // Scroll memory window.
      oled.scrollMemory(oled.fontRows());
      // jump to line start
      oled.setCol(0);
      // print line and clean up behind
      oled.print(oledData.line);
      oled.clearToEOL();
    }
  }
}
