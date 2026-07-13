# PolyHome — Embedded Home-Automation Controller

**Embedded System Programming — Project Report**
Author: _[your name]_ · Course contact: barthelemy.heyrman@ube.fr
Date: _[defense date, end of August 2026]_

---

## 1. Introduction & Requirements Analysis

The goal of this project is to design and prototype a dedicated **home-automation
controller device** for a connected chalet equipped with shutters, a garage door
and lights. The house hub already exposes a REST web API; a virtual mock-up of
the house is available at `https://polyhome.lesmoulinsdudev.com?houseId=<id>`.

**Required features (from the assignment):**

| # | Requirement | Status |
|---|---|---|
| R1 | Create a user account from the device | ☐ TODO |
| R2 | Automatic login after account creation | ☐ TODO |
| R3 | Store the authentication token on the device (persistent) | ☐ TODO |
| R4 | List the devices present in the house | ☐ TODO |
| R5 | Send commands to the devices | ☐ TODO |
| B1 | *(bonus)* Scenes: control several devices with one action | ☐ TODO |
| B2 | *(bonus)* Automatic mode using sensors (light/temperature) | ☐ TODO |

The device must include a **display**, a **keyboard**, and every supporting
component. The prototype is designed in **Cirkit Designer**.

## 2. System Architecture

_TODO: block diagram — Controller device ↔ WiFi ↔ PolyHome REST API ↔ virtual house._

The controller is built around an **ESP32 DevKit**, chosen because the API is
only reachable over HTTPS, so the microcontroller needs built-in WiFi and
enough RAM/flash for TLS. User interaction goes through a 20x4 I2C LCD and a
4x4 matrix keypad.

## 3. Hardware Design

| Component | Role | Interface / pins |
|---|---|---|
| ESP32 DevKit | MCU + WiFi/HTTPS client | — |
| LCD 20x4 (I2C backpack) | Menu & status display | SDA = GPIO21, SCL = GPIO22 |
| Matrix keypad 4x4 | Text entry & menu navigation | 8 GPIOs (rows + columns), strapping pins avoided |
| _(bonus)_ LDR + resistor divider | Ambient light for auto mode | 1 ADC pin |

_TODO: insert Cirkit Designer schematic export + link to the project._

**Component choice justification:** _TODO (I2C saves pins; 4 LCD lines fit a
menu; keypad A–D keys used as function keys; multi-tap text entry on digits)._

## 4. Software Design

The firmware is organized in modules:

```
src/
├── main.cpp          # setup/loop, state-machine dispatch
├── api_client.{h,cpp}# all HTTP calls to the PolyHome API
├── ui.{h,cpp}        # LCD rendering, menu screens
├── keypad_input.{h,cpp} # non-blocking scan, multi-tap text entry
├── storage.{h,cpp}   # NVS (Preferences): token, login, houseId
└── config.h          # WiFi credentials, API base URL
```

The UI is a **finite state machine** (WIFI_CONNECT → LOGIN_MENU → ENTER_LOGIN →
ENTER_PASSWORD → HOUSE_SELECT → DEVICE_LIST → DEVICE_COMMAND → …), driven by a
single `switch` in `loop()` with `millis()`-based timing (no blocking
`delay()`), so keypad input stays responsive.

_TODO: state-machine diagram._
_TODO: auth sequence diagram (register → auth → store token → houses → devices → command)._

## 5. API Integration

Base URL: `https://polyhome.lesmoulinsdudev.com/api`

| Endpoint | Method | Purpose | Notes |
|---|---|---|---|
| `/users/register` | POST | Create account | Returns **no token**; 409 if login taken |
| `/users/auth` | POST | Login | Only endpoint returning `{ "token": … }` |
| `/houses` | GET (Bearer) | List houses of the user | `houseId` fetched at runtime, never hardcoded |
| `/houses/<id>/devices` | GET (Bearer) | List devices | House page must be open in a browser |
| `/houses/<id>/devices/<devId>/command` | POST (Bearer) | Send a command | Command must be one of the device's `availableCommands` |

**Token lifecycle:** on boot the token is read from NVS and validated with
`GET /houses`; on `403` the device re-authenticates with the stored credentials
and saves the new token; with no stored account it shows the
Create-account/Login menu. After registration the device immediately calls
`/users/auth` (registration itself returns no token).

**Error handling:** _TODO — table of HTTP codes → user-visible messages._

**Security note:** for this prototype TLS is used with certificate verification
disabled (`setInsecure()`); a production device would pin the server
certificate. Credentials stored in NVS are not encrypted — acceptable for a
course prototype, discussed in §8.

## 6. Bonus Features

_TODO after implementation:_
- **Scenes** ("Leaving home" = close all shutters + garage + lights off): one
  keypad key iterates over the device list filtered by `type`.
- **Auto mode (LDR)**: below the darkness threshold → lights on; hysteresis
  (two thresholds) prevents oscillation; manual override supported.

## 7. Tests & Results

### 7.1 API validation (before firmware)
The full API flow was first validated with `curl` scripts
(see `api-tests/phase1_test.sh`): register → auth → houses → devices → command,
observing the virtual house react in real time.

_TODO: transcript + screenshots of the virtual house reacting._

### 7.2 Firmware tests
_TODO: test table — each requirement R1–R5, how it was tested, result._

## 8. Difficulties Encountered & Solutions

| Difficulty | Solution |
|---|---|
| PolyHome server outage (HTTP 503, 12 Jul 2026) during early API testing | Prepared automated test scripts + availability watcher; validated flow once service returned |
| Device list returns nothing unless a browser tab has the house open | Documented as an operating constraint; UI shows a hint message |
| _TODO: add as they come (keypad text entry, TLS memory, Cirkit simulation limits…)_ | |

## 9. Conclusion & Possible Improvements

_TODO: summary; improvements — certificate pinning, encrypted credential
storage, OTA updates, richer display (TFT), MQTT push instead of polling._

---

## Appendix A — Links

- GitHub repository: _TODO_
- Cirkit Designer project: _TODO_
- Virtual house: `https://polyhome.lesmoulinsdudev.com?houseId=<id>`
