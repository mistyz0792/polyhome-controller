#pragma once
#include <Arduino.h>

// 20x4 I2C LCD rendering helpers. All drawing goes through here so the
// display type can be swapped without touching the state machine.

void uiInit();
void uiClear();
void uiLine(uint8_t row, const String &text);          // pad/truncate to 20 cols
void uiScreen(const String &l0, const String &l1 = "",
              const String &l2 = "", const String &l3 = "");

// Scrolling menu: draws `title` + up to 3 visible items with a '>' cursor.
void uiMenu(const String &title, const String items[], uint8_t count,
            uint8_t selected);
