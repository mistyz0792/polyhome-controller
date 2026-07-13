# PolyHome — Embedded Home-Automation Controller

Course project for **Embedded System Programming**: a prototype controller
device (ESP32-S3 + LCD 16x2 + 4x4 keypad, designed in Cirkit Designer) that
drives a connected chalet — shutters, garage door and lights — through the
PolyHome REST API at `https://polyhome.lesmoulinsdudev.com`.

## Repository layout

| Folder | Content |
|---|---|
| [`firmware/`](firmware/) | ESP32 firmware — single-file `polyhome.ino` for Cirkit Designer + modular `src/` version. See its README for wiring & controls |
| [`webapp/`](webapp/) | Web dashboard (`index.html`, direct API access) and web remote (`remote.html`, controls the board itself over MQTT) + local dev proxy `server.py` |
| [`api-tests/`](api-tests/) | `curl` smoke-test script used to validate the API flow |
| [`report/`](report/) | Project report (work in progress) |

## Feature summary

- Create account / login from the device keypad (multi-tap text entry)
- Automatic login after registration; **auth token persisted in NVS**
- Device list + commands built dynamically from `availableCommands`
- Silent re-login on HTTP 403
- **Bonus — scene**: one key closes shutters + garage and switches lights off
- **Bonus — web remote**: `webapp/remote.html` controls the running board via
  MQTT (the page locks itself the moment the simulation stops)

## Links

- Cirkit Designer project: _TODO_
- Live dashboard (Vercel): _TODO_
- Virtual house: `https://polyhome.lesmoulinsdudev.com?houseId=<id>`
