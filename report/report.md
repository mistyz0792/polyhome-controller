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
| R1 | Create a user account from the device | ✅ implemented — pending live API test |
| R2 | Automatic login after account creation | ✅ implemented — pending live API test |
| R3 | Store the authentication token on the device (persistent) | ✅ implemented (NVS) |
| R4 | List the devices present in the house | ✅ implemented — pending live API test |
| R5 | Send commands to the devices | ✅ implemented — pending live API test |
| B1 | *(bonus)* Scene "Leaving home": one key controls the whole house | ✅ implemented |
| B2 | *(bonus)* Automatic mode using a light sensor (LDR) | ✅ implemented |
| B3 | *(bonus)* Web remote controlling the board over MQTT, with accounts stored on the device | ✅ implemented & deployed |

*"Pending live API test": the PolyHome server has been returning HTTP 503
since 13 July; every flow has been validated up to the API call itself.*

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
| ESP32-S3 DevKit | MCU + WiFi (HTTPS + MQTT client) | — |
| LCD 16x2 (I2C backpack, addr 0x27) | Menu & status display | SDA = GPIO8, SCL = GPIO9, VCC = VIN(5V) |
| Matrix keypad 4x4 | Text entry & menu navigation | rows GPIO 4/5/6/7, columns GPIO 15/16/17/18 |
| LDR comparator module | Ambient light for auto mode | D0 = GPIO46, VCC = 3V3 |

_TODO: insert Cirkit Designer schematic export + link to the project._

**Component choice justification:** the ESP32-S3 was chosen because the API is
only reachable over HTTPS, requiring built-in WiFi and enough RAM for TLS; the
Cirkit Designer simulator emulates it instruction-accurately including network
traffic. I2C keeps the display at 2 pins; the keypad's A–D keys serve as
function keys (menu up/down, scene, back) while digits provide multi-tap text
entry. The LDR module's on-board comparator (potentiometer threshold) provides
hardware hysteresis for the auto mode. Column pins were chosen among GPIOs
with internal pull-ups — input-only pins 34-39 (classic ESP32) cannot scan a
keypad matrix, which drove an early pin-map revision.

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

**B1 — Scene "Leaving home"** (key `C` in the device list): iterates over the
live device list and sends each device its "safe" command (`CLOSE` /
`TURN OFF`), so shutters and garage close and lights switch off with a single
key press. Commands are picked from each device's `availableCommands`, never
hardcoded.

**B2 — Auto mode with light sensor** (key `0` toggles, `[AUTO]` shown in the
title): the LDR module's digital output drives the lights — dark → lights on,
bright → lights off. The module's comparator potentiometer provides hardware
hysteresis, and the firmware adds a 3-second stability window so brief shadows
do not toggle the house. Manual control stays available while armed.

**B3 — Web remote with board-managed accounts**
(https://polyhome-controller.vercel.app/remote.html): because the simulator
only allows outbound connections, the board maintains an MQTT link to a broker
and the web page talks to the same broker — the ESP32 effectively *is* the
server. An MQTT last-will flips the page to a locked "board offline" state the
instant the simulation stops. User accounts for the remote are registered from
the web page but **stored in the board's NVS**; the board verifies credentials
and issues in-RAM session tokens, so an unauthenticated browser can see the
board but cannot command it. A session dies with a board reboot, mirroring the
PolyHome 403 → re-login lifecycle.

A separate direct dashboard (https://polyhome-controller.vercel.app) was also
built as a development tool: it exercises the same API endpoints as the
firmware (register → auth → token in localStorage → devices → commands) and
was used to debug the API flow independently of the hardware.

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
| PolyHome server outage (HTTP 503, from 13 Jul 2026) blocking all API testing | Reported to the course contact; built curl test scripts, a web dashboard and an MQTT test path so every layer except the final API call could still be validated |
| Arduino `.ino` preprocessor hoists auto-generated prototypes above `enum` declarations → "does not name a type" errors | Functions exchanging state/event values use `int` in their signatures; enums keep the named constants |
| Keypad columns need internal pull-ups; input-only pins (34-39 on classic ESP32) silently break the matrix scan | Pin map revised; final ESP32-S3 layout uses pull-up-capable GPIOs for all columns |
| Moving from a 20x4 to a 16x2 LCD broke every screen layout | UI layer rewritten to be size-agnostic: scrolling menu shows `LCD_ROWS-1` items, all writes clipped to the real display size |
| Cirkit simulator cannot accept inbound connections, so the board cannot host a web server directly | Reversed the flow with MQTT: board keeps an outbound broker connection; the web remote publishes to the same topics (see §6 B3) |
| Public MQTT broker means anyone could command the board | Board-side account store + session tokens; noted that production would need a private broker + TLS + hashed passwords |

## 9. Conclusion & Possible Improvements

_TODO: summary; improvements — certificate pinning, encrypted credential
storage, OTA updates, richer display (TFT), MQTT push instead of polling._

---

## Appendix A — Links

- GitHub repository: _TODO_
- Cirkit Designer project: _TODO_
- Virtual house: `https://polyhome.lesmoulinsdudev.com?houseId=<id>`
