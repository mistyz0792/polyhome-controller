// PolyHome controller — ESP32 + 20x4 I2C LCD + 4x4 keypad
//
// Keys: A/B = up/down   # = select/validate   D = back/cancel
//       * = backspace (editing) / refresh (device list)   C = scene "Leaving home"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "api_client.h"
#include "keypad_input.h"
#include "ui.h"

enum State {
  ST_WIFI_CONNECT,
  ST_BOOT_AUTH,      // try stored token / stored credentials
  ST_ACCOUNT_MENU,   // Login | Register
  ST_ENTER_LOGIN,
  ST_ENTER_PASSWORD,
  ST_SUBMIT,         // perform register(+login) or login, store token
  ST_LOAD_DEVICES,
  ST_DEVICE_LIST,
  ST_COMMAND_MENU,
  ST_RUN_SCENE,
  ST_MESSAGE         // show text until '#'
};

static State state = ST_WIFI_CONNECT;
static State afterMessage = ST_ACCOUNT_MENU;

static String gToken, gLogin, gPassword;
static int gHouseId = -1;
static bool gRegistering = false;

static MultiTapEntry entry;

// device list cache
static JsonDocument gDevices;
static uint8_t devCount = 0, devSel = 0, cmdSel = 0;

static void showMessage(const String &l0, const String &l1, State next) {
  uiScreen(l0, l1, "", "        [#] OK");
  afterMessage = next;
  state = ST_MESSAGE;
}

static JsonObject deviceAt(uint8_t i) {
  return gDevices["devices"][i].as<JsonObject>();
}

// ---------- device list ----------

static void drawDeviceList() {
  String items[16];
  for (uint8_t i = 0; i < devCount && i < 16; i++) {
    JsonObject d = deviceAt(i);
    String label = d["id"].as<String>();
    if (!d["power"].isNull())
      label += d["power"].as<int>() ? " [ON]" : " [off]";
    else if (!d["opening"].isNull())
      label += " " + String(d["opening"].as<int>()) + "%";
    items[i] = label;
  }
  uiMenu("Devices  C=scene", items, devCount, devSel);
}

static void drawCommandMenu() {
  JsonObject d = deviceAt(devSel);
  JsonArray cmds = d["availableCommands"].as<JsonArray>();
  String items[12];
  uint8_t n = 0;
  for (JsonVariant c : cmds) if (n < 12) items[n++] = c.as<String>();
  uiMenu(d["id"].as<String>(), items, n, cmdSel);
}

static uint8_t commandCount() {
  return deviceAt(devSel)["availableCommands"].as<JsonArray>().size();
}

// ---------- scene bonus: "Leaving home" ----------

static void runLeavingHomeScene() {
  uiScreen("Scene: Leaving home", "Sending commands...");
  uint8_t done = 0;
  for (uint8_t i = 0; i < devCount; i++) {
    JsonObject d = deviceAt(i);
    String type = d["type"].as<String>();
    JsonArray cmds = d["availableCommands"].as<JsonArray>();
    // pick the "make it safe/off" command per device type
    String want = "";
    for (JsonVariant c : cmds) {
      String cmd = c.as<String>();
      if (cmd.equalsIgnoreCase("CLOSE") || cmd.equalsIgnoreCase("TURN OFF") ||
          cmd.equalsIgnoreCase("OFF")) { want = cmd; break; }
    }
    if (want.length()) {
      apiSendCommand(gToken, gHouseId, d["id"].as<String>(), want);
      done++;
      uiLine(2, String(done) + " devices done");
    }
  }
  showMessage("Scene finished", String(done) + " commands sent", ST_LOAD_DEVICES);
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  storageInit();
  uiInit();
  keypadInit();
  uiScreen("PolyHome control", "Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  state = ST_WIFI_CONNECT;
}

void loop() {
  switch (state) {

  case ST_WIFI_CONNECT: {
    static unsigned long t0 = millis();
    if (WiFi.status() == WL_CONNECTED) {
      uiScreen("WiFi connected", WiFi.localIP().toString());
      state = ST_BOOT_AUTH;
    } else if (millis() - t0 > 20000) {
      t0 = millis();
      WiFi.reconnect();
      uiLine(2, "retrying...");
    }
    break;
  }

  case ST_BOOT_AUTH: {
    gToken = storageLoadToken();
    if (gToken.length()) {                       // validate stored token
      uiScreen("Checking token...");
      int code = apiGetHouses(gToken, gHouseId);
      if (code == 200 && gHouseId >= 0) {
        storageSaveHouseId(gHouseId);
        state = ST_LOAD_DEVICES;
        break;
      }
    }
    if (storageLoadAccount(gLogin, gPassword)) { // re-login with saved account
      uiScreen("Logging in...", gLogin);
      int code = apiLogin(gLogin, gPassword, gToken);
      if (code == 200) {
        storageSaveToken(gToken);
        state = ST_SUBMIT;   // ST_SUBMIT finishes by fetching houses
        gRegistering = false;
        break;
      }
    }
    state = ST_ACCOUNT_MENU;
    break;
  }

  case ST_ACCOUNT_MENU: {
    static uint8_t sel = 0;
    static bool drawn = false;
    if (!drawn) {
      String items[2] = {"Login", "Create account"};
      uiMenu("PolyHome  A/B # ok", items, 2, sel);
      drawn = true;
    }
    char k = keypadGetKey();
    if (k == 'A' || k == 'B') { sel = 1 - sel; drawn = false; }
    if (k == '#') {
      gRegistering = (sel == 1);
      entry.reset(false);
      state = ST_ENTER_LOGIN;
      drawn = false;
      uiScreen(gRegistering ? "New account login:" : "Login:", "",
               "2-9 letters *del A=12", "# ok        D cancel");
    }
    break;
  }

  case ST_ENTER_LOGIN: {
    EntryEvent e = entry.update();
    if (e == ENTRY_CHANGED) uiLine(1, entry.textWithCursor());
    if (e == ENTRY_CANCELLED) state = ST_ACCOUNT_MENU;
    if (e == ENTRY_DONE && entry.value().length() > 0) {
      gLogin = entry.value();
      entry.reset(false);
      uiScreen("Password:", "", "2-9 letters *del A=12", "# ok        D cancel");
      state = ST_ENTER_PASSWORD;
    }
    break;
  }

  case ST_ENTER_PASSWORD: {
    EntryEvent e = entry.update();
    if (e == ENTRY_CHANGED) uiLine(1, entry.textWithCursor());
    if (e == ENTRY_CANCELLED) state = ST_ACCOUNT_MENU;
    if (e == ENTRY_DONE && entry.value().length() > 0) {
      gPassword = entry.value();
      state = ST_SUBMIT;
    }
    break;
  }

  case ST_SUBMIT: {
    if (gRegistering) {
      uiScreen("Creating account...");
      int code = apiRegister(gLogin, gPassword);
      if (code == 409) { showMessage("Login already used", "", ST_ACCOUNT_MENU); break; }
      if (code != 200) { showMessage("Register failed", "HTTP " + String(code), ST_ACCOUNT_MENU); break; }
      // register returns no token -> log in right away (required behavior)
    }
    if (gToken.length() == 0) {
      uiScreen("Logging in...");
      int code = apiLogin(gLogin, gPassword, gToken);
      if (code != 200) { showMessage("Login failed", "HTTP " + String(code), ST_ACCOUNT_MENU); break; }
    }
    storageSaveAccount(gLogin, gPassword);
    storageSaveToken(gToken);
    uiScreen("Fetching house...");
    int code = apiGetHouses(gToken, gHouseId);
    if (code != 200 || gHouseId < 0) {
      gToken = "";
      showMessage("House fetch failed", "HTTP " + String(code), ST_ACCOUNT_MENU);
      break;
    }
    storageSaveHouseId(gHouseId);
    state = ST_LOAD_DEVICES;
    break;
  }

  case ST_LOAD_DEVICES: {
    uiScreen("Loading devices...");
    int code = apiGetDevices(gToken, gHouseId, gDevices);
    if (code == 403) {                 // token expired -> transparent re-login
      gToken = "";
      state = ST_BOOT_AUTH;
      break;
    }
    if (code != 200) {
      showMessage("Device list failed", "Open house in web?", ST_DEVICE_LIST);
      break;
    }
    devCount = gDevices["devices"].as<JsonArray>().size();
    if (devSel >= devCount) devSel = 0;
    if (devCount == 0) {
      showMessage("No devices found", "Open house in web!", ST_LOAD_DEVICES);
      break;
    }
    drawDeviceList();
    state = ST_DEVICE_LIST;
    break;
  }

  case ST_DEVICE_LIST: {
    char k = keypadGetKey();
    if (k == 'A' && devSel > 0)            { devSel--; drawDeviceList(); }
    if (k == 'B' && devSel + 1 < devCount) { devSel++; drawDeviceList(); }
    if (k == '*') state = ST_LOAD_DEVICES;           // refresh states
    if (k == 'C') state = ST_RUN_SCENE;
    if (k == '#') { cmdSel = 0; drawCommandMenu(); state = ST_COMMAND_MENU; }
    break;
  }

  case ST_COMMAND_MENU: {
    char k = keypadGetKey();
    if (k == 'A' && cmdSel > 0)                 { cmdSel--; drawCommandMenu(); }
    if (k == 'B' && cmdSel + 1 < commandCount()) { cmdSel++; drawCommandMenu(); }
    if (k == 'D') { drawDeviceList(); state = ST_DEVICE_LIST; }
    if (k == '#') {
      JsonObject d = deviceAt(devSel);
      String cmd = d["availableCommands"][cmdSel].as<String>();
      uiScreen("Sending:", cmd);
      int code = apiSendCommand(gToken, gHouseId, d["id"].as<String>(), cmd);
      if (code == 403) { gToken = ""; state = ST_BOOT_AUTH; break; }
      showMessage(code == 200 ? "Command sent" : "Send failed",
                  d["id"].as<String>() + " " + cmd, ST_LOAD_DEVICES);
    }
    break;
  }

  case ST_RUN_SCENE:
    runLeavingHomeScene();
    break;

  case ST_MESSAGE:
    if (keypadGetKey() == '#') state = afterMessage;
    break;
  }
}
