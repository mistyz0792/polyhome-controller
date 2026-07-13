# PolyHome Controller — ESP32 Firmware

Prototype home-automation controller for the PolyHome virtual chalet
(Embedded System Programming project). Talks to the REST API at
`https://polyhome.lesmoulinsdudev.com/api`.

## Hardware

| Component | Interface | ESP32-S3 pins |
|---|---|---|
| ESP32-S3 DevKit | — | — |
| LCD 16x2 (I2C backpack, addr `0x27`) | I2C | SDA=8, SCL=9 · VCC=VIN(5V), GND |
| Keypad 4x4 | GPIO matrix | rows: 4, 5, 6, 7 · cols: 15, 16, 17, 18 |
| LDR module (bonus, auto mode) | digital D0 | 46 · VCC=3V3, GND |

Note: GPIO 46 is an S3 strapping pin — fine in the Cirkit simulator, but on
real hardware move the LDR D0 wire (e.g. to GPIO 10) if the board fails to boot.

## Libraries

- `ArduinoJson`
- `LiquidCrystal_I2C`
- (WiFi / WiFiClientSecure / HTTPClient / Preferences ship with the ESP32 core)

## Setup

1. Edit `src/config.h`: WiFi SSID/password.
2. Build & flash (Arduino IDE or PlatformIO). For Cirkit Designer the modules
   can be flattened into a single sketch if the IDE requires one file.
3. **Open the house in a browser** — the API only reports devices while at
   least one tab shows `https://polyhome.lesmoulinsdudev.com?houseId=<id>`.

## Controls

| Key | Meaning |
|---|---|
| `A` / `B` | menu up / down (in editor: toggle abc/123) |
| `#` | select / validate |
| `D` | back / cancel |
| `*` | backspace (editor) · refresh (device list) |
| `C` | scene **"Leaving home"** (close shutters + garage, lights off) |
| `2`–`9` | multi-tap letters (press `2` → a, b, c, 2) |

## Behavior (assignment requirements)

- First boot: **Create account / Login** menu → credentials typed on the keypad.
- After registration the device immediately calls `/users/auth`
  (register returns no token) and **stores the token in NVS** (`Preferences`).
- Every following boot logs in automatically: stored token is validated with
  `GET /houses`; on `403` the device re-authenticates with stored credentials.
- Device list is fetched live; the command menu is built from each device's
  `availableCommands` (nothing hardcoded).

## Module layout

```
src/
├── main.cpp          # state machine (see report §4)
├── api_client.{h,cpp}# REST calls, returns HTTP codes to the UI
├── ui.{h,cpp}        # LCD rendering
├── keypad_input.{h,cpp} # matrix scan + multi-tap editor
├── storage.{h,cpp}   # NVS: token, credentials, houseId
└── config.h          # WiFi + pins
```
