#pragma once

// ---------- WiFi ----------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ---------- PolyHome API ----------
#define API_BASE "https://polyhome.lesmoulinsdudev.com/api"

// ---------- Hardware pins ----------
// I2C LCD 20x4
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define LCD_ADDR    0x27
#define LCD_COLS    20
#define LCD_ROWS    4

// 4x4 matrix keypad (strapping pins 0/2/12/15 avoided)
#define KP_R1 13
#define KP_R2 14
#define KP_R3 27
#define KP_R4 26
#define KP_C1 25
#define KP_C2 33
#define KP_C3 32
#define KP_C4 4

// Bonus: LDR on ADC1 (ADC2 is unusable while WiFi is on)
#define PIN_LDR 34
