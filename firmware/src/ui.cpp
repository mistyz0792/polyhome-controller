#include "ui.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void uiInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
}

void uiClear() { lcd.clear(); }

void uiLine(uint8_t row, const String &text) {
  String s = text.substring(0, LCD_COLS);
  while (s.length() < LCD_COLS) s += ' ';
  lcd.setCursor(0, row);
  lcd.print(s);
}

void uiScreen(const String &l0, const String &l1,
              const String &l2, const String &l3) {
  uiLine(0, l0); uiLine(1, l1); uiLine(2, l2); uiLine(3, l3);
}

void uiMenu(const String &title, const String items[], uint8_t count,
            uint8_t selected) {
  uiLine(0, title);
  uint8_t first = (selected > 2) ? selected - 2 : 0;   // keep cursor visible
  for (uint8_t row = 1; row < LCD_ROWS; row++) {
    uint8_t i = first + row - 1;
    if (i < count)
      uiLine(row, String(i == selected ? ">" : " ") + items[i]);
    else
      uiLine(row, "");
  }
}
