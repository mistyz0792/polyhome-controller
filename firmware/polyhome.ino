// ============================================================================
//  PolyHome controller — ESP32 + LCD 20x4 (I2C) + 4x4 keypad (+ LDR bonus)
//  Single-file build for Cirkit Designer.
//
//  Libraries to add in Cirkit / Arduino IDE:
//    - ArduinoJson
//    - LiquidCrystal I2C  (by Frank de Brabander)
//
//  Keys:  A/B = up/down   # = select/validate   D = back/cancel
//         * = backspace (editing) / refresh (device list)
//         C = scene "Leaving home"
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ============================== CONFIG ======================================

// Cirkit simulator: virtual open AP "CirkitWifi" (no password).
// On real hardware replace with your own network.
#define WIFI_SSID     "CirkitWifi"
#define WIFI_PASSWORD ""
#define API_BASE      "https://polyhome.lesmoulinsdudev.com/api"

// MQTT remote control (webapp/remote.html) — public broker, so pick a
// unique prefix nobody else would guess.
#define MQTT_HOST   "broker.hivemq.com"
#define MQTT_PORT   1883
#define MQTT_PREFIX "polyhome-kdijon26"

// LCD 16x2 on I2C (ESP32-S3)
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define LCD_ADDR    0x27
#define LCD_COLS    16
#define LCD_ROWS    2

// 4x4 keypad (ESP32-S3)
#define KP_R1 4
#define KP_R2 5
#define KP_R3 6
#define KP_R4 7
#define KP_C1 15
#define KP_C2 16
#define KP_C3 17
#define KP_C4 18

// LDR module digital output (bonus auto mode): HIGH/LOW past the pot threshold
// !! GPIO46 is an S3 strapping pin — fine in the simulator; on real hardware
//    move it (e.g. GPIO 10) if the board fails to boot.
#define PIN_LDR_DO 46

// ============================== STORAGE (NVS) ===============================

Preferences prefs;

void storageInit()                     { prefs.begin("polyhome", false); }
void storageSaveAccount(const String &l, const String &p) {
  prefs.putString("login", l); prefs.putString("password", p);
}
bool storageLoadAccount(String &l, String &p) {
  l = prefs.getString("login", ""); p = prefs.getString("password", "");
  return l.length() > 0 && p.length() > 0;
}
void   storageSaveToken(const String &t) { prefs.putString("token", t); }
String storageLoadToken()                { return prefs.getString("token", ""); }
void   storageSaveHouseId(int id)        { prefs.putInt("houseId", id); }
int    storageLoadHouseId()              { return prefs.getInt("houseId", -1); }

// ============================== API CLIENT ==================================
// Every call returns the HTTP status code (-1 = connection failure) so the UI
// can react: 403 -> re-login, 409 -> login taken, etc.

WiFiClientSecure tlsClient;
bool tlsReady = false;

void ensureTls() {
  if (!tlsReady) { tlsClient.setInsecure(); tlsReady = true; }
  // setInsecure(): certificate not verified — fine for the prototype,
  // a production device would pin the server certificate.
}

int httpPost(const String &url, const String &body, const String &token, String &resp) {
  ensureTls();
  HTTPClient http;
  if (!http.begin(tlsClient, url)) return -1;
  http.addHeader("Content-Type", "application/json");
  if (token.length()) http.addHeader("Authorization", "Bearer " + token);
  int code = http.POST(body);
  resp = (code > 0) ? http.getString() : String("");
  http.end();
  return code;
}

int httpGet(const String &url, const String &token, String &resp) {
  ensureTls();
  HTTPClient http;
  if (!http.begin(tlsClient, url)) return -1;
  if (token.length()) http.addHeader("Authorization", "Bearer " + token);
  int code = http.GET();
  resp = (code > 0) ? http.getString() : String("");
  http.end();
  return code;
}

String credentialsBody(const String &login, const String &password) {
  JsonDocument d;
  d["login"] = login; d["password"] = password;
  String s; serializeJson(d, s); return s;
}

// POST /users/register — returns NO token; apiLogin() must follow.
int apiRegister(const String &login, const String &password) {
  String r;
  return httpPost(String(API_BASE) + "/users/register",
                  credentialsBody(login, password), "", r);
}

// POST /users/auth — the only endpoint that returns a token.
int apiLogin(const String &login, const String &password, String &tokenOut) {
  String r;
  int code = httpPost(String(API_BASE) + "/users/auth",
                      credentialsBody(login, password), "", r);
  tokenOut = "";
  if (code == 200) {
    JsonDocument d;
    if (deserializeJson(d, r) == DeserializationError::Ok)
      tokenOut = d["token"].as<String>();
    if (tokenOut.length() == 0) return -1;
  }
  return code;
}

// GET /houses — picks the house with owner == true.
int apiGetHouses(const String &token, int &houseIdOut) {
  String r;
  int code = httpGet(String(API_BASE) + "/houses", token, r);
  houseIdOut = -1;
  if (code == 200) {
    JsonDocument d;
    if (deserializeJson(d, r) == DeserializationError::Ok) {
      for (JsonObject h : d.as<JsonArray>())
        if (h["owner"].as<bool>()) { houseIdOut = h["houseId"].as<int>(); break; }
      if (houseIdOut < 0 && d.as<JsonArray>().size() > 0)   // guest fallback
        houseIdOut = d.as<JsonArray>()[0]["houseId"].as<int>();
    }
  }
  return code;
}

// GET /houses/<id>/devices — needs the house open in a browser tab!
int apiGetDevices(const String &token, int houseId, JsonDocument &out) {
  String r;
  int code = httpGet(String(API_BASE) + "/houses/" + houseId + "/devices", token, r);
  if (code == 200 && deserializeJson(out, r) != DeserializationError::Ok) return -1;
  return code;
}

// POST /houses/<id>/devices/<devId>/command
int apiSendCommand(const String &token, int houseId,
                   const String &deviceId, const String &command) {
  JsonDocument d; d["command"] = command;
  String body, r; serializeJson(d, body);
  return httpPost(String(API_BASE) + "/houses/" + houseId +
                  "/devices/" + deviceId + "/command", body, token, r);
}

// ============================== KEYPAD ======================================

const char KEYMAP[4][4] = {
  {'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'}
};
const uint8_t KP_ROWS[4] = {KP_R1, KP_R2, KP_R3, KP_R4};
const uint8_t KP_COLS[4] = {KP_C1, KP_C2, KP_C3, KP_C4};

char kpLast = 0;
unsigned long kpEdge = 0;

void keypadInit() {
  for (uint8_t r = 0; r < 4; r++) { pinMode(KP_ROWS[r], OUTPUT); digitalWrite(KP_ROWS[r], HIGH); }
  for (uint8_t c = 0; c < 4; c++) pinMode(KP_COLS[c], INPUT_PULLUP);
}

char keypadScan() {
  char found = 0;
  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(KP_ROWS[r], LOW);
    for (uint8_t c = 0; c < 4; c++)
      if (digitalRead(KP_COLS[c]) == LOW) found = KEYMAP[r][c];
    digitalWrite(KP_ROWS[r], HIGH);
  }
  return found;
}

// non-blocking, debounced, fires once per press
char keypadGetKey() {
  char now = keypadScan();
  unsigned long t = millis();
  if (now != kpLast) {
    if (t - kpEdge < 30) return 0;
    kpEdge = t; kpLast = now;
    if (now) return now;
  }
  return 0;
}

// ---------- multi-tap text editor (press 2 -> a,b,c,2) ----------

const char *TAPS[10] = {"0 ","1","abc2","def3","ghi4","jkl5","mno6","pqrs7","tuv8","wxyz9"};

enum EntryEvent { ENTRY_IDLE, ENTRY_CHANGED, ENTRY_DONE, ENTRY_CANCELLED };

String  entryText;
char    entryPending = 0;
uint8_t entryTap = 0;
unsigned long entryLastTap = 0;
bool    entryDigits = false;

void entryReset(bool digitsOnly = false) {
  entryText = ""; entryPending = 0; entryTap = 0; entryDigits = digitsOnly;
}

void entryCommit() {
  if (entryPending) {
    const char *set = TAPS[entryPending - '0'];
    entryText += set[entryTap % strlen(set)];
    entryPending = 0; entryTap = 0;
  }
}

// returns an EntryEvent value; typed int because the Arduino .ino preprocessor
// hoists auto-generated prototypes above the enum declaration
int entryUpdate() {
  if (entryPending && millis() - entryLastTap > 900) { entryCommit(); return ENTRY_CHANGED; }
  char k = keypadGetKey();
  if (!k) return ENTRY_IDLE;
  if (k == '#') { entryCommit(); return ENTRY_DONE; }
  if (k == 'D') return ENTRY_CANCELLED;
  if (k == '*') {
    if (entryPending) { entryPending = 0; entryTap = 0; }
    else if (entryText.length()) entryText.remove(entryText.length() - 1);
    return ENTRY_CHANGED;
  }
  if (k == 'A') { entryCommit(); entryDigits = !entryDigits; return ENTRY_CHANGED; }
  if (k < '0' || k > '9') return ENTRY_IDLE;
  if (entryDigits) { entryCommit(); entryText += k; return ENTRY_CHANGED; }
  if (k == entryPending) entryTap++;
  else { entryCommit(); entryPending = k; entryTap = 0; }
  entryLastTap = millis();
  return ENTRY_CHANGED;
}

String entryDisplay() {
  String s = entryText;
  if (entryPending) {
    const char *set = TAPS[entryPending - '0'];
    s += set[entryTap % strlen(set)];
  }
  return s;
}

// ============================== UI (LCD) ====================================

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void uiInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
}

void uiLine(uint8_t row, const String &text) {
  if (row >= LCD_ROWS) return;
  String s = text.substring(0, LCD_COLS);
  while (s.length() < LCD_COLS) s += ' ';
  lcd.setCursor(0, row);
  lcd.print(s);
}

void uiScreen(const String &a, const String &b = "", const String &c = "", const String &d = "") {
  uiLine(0, a); uiLine(1, b); uiLine(2, c); uiLine(3, d);
}

// Works on any LCD size: title on row 0, scrolling items below.
// On 16x2 that means one visible item — A/B scrolls through the list.
void uiMenu(const String &title, const String items[], uint8_t count, uint8_t sel) {
  uiLine(0, title);
  uint8_t visible = LCD_ROWS - 1;
  uint8_t first = (sel >= visible) ? sel - visible + 1 : 0;
  for (uint8_t row = 1; row < LCD_ROWS; row++) {
    uint8_t i = first + row - 1;
    uiLine(row, i < count ? String(i == sel ? ">" : " ") + items[i] : String(""));
  }
}

// ============================== STATE MACHINE ===============================

enum State {
  ST_WIFI_CONNECT, ST_BOOT_AUTH, ST_ACCOUNT_MENU,
  ST_ENTER_LOGIN, ST_ENTER_PASSWORD, ST_SUBMIT,
  ST_LOAD_DEVICES, ST_DEVICE_LIST, ST_COMMAND_MENU,
  ST_RUN_SCENE, ST_MESSAGE
};

int state = ST_WIFI_CONNECT;        // holds State values (int: see entryUpdate note)
int afterMessage = ST_ACCOUNT_MENU;

String gToken, gLogin, gPassword;
int  gHouseId = -1;
bool gRegistering = false;

JsonDocument gDevices;
uint8_t devCount = 0, devSel = 0, cmdSel = 0;

void showMessage(const String &l0, const String &l1, int next) {
  if (LCD_ROWS >= 4) uiScreen(l0, l1, "", "        [#] OK");
  else               uiScreen(l0, l1.length() ? l1 : "[#] OK");
  afterMessage = next;
  state = ST_MESSAGE;
}

JsonObject deviceAt(uint8_t i) { return gDevices["devices"][i].as<JsonObject>(); }
uint8_t commandCount() { return deviceAt(devSel)["availableCommands"].as<JsonArray>().size(); }

void drawDeviceList() {
  String items[16];
  for (uint8_t i = 0; i < devCount && i < 16; i++) {
    JsonObject d = deviceAt(i);
    String label = d["id"].as<String>();
    if (!d["power"].isNull())        label += d["power"].as<int>() ? " [ON]" : " [off]";
    else if (!d["opening"].isNull()) label += " " + String(d["opening"].as<int>()) + "%";
    items[i] = label;
  }
  uiMenu("Devices  C=scene", items, devCount, devSel);
}

void drawCommandMenu() {
  JsonObject d = deviceAt(devSel);
  JsonArray cmds = d["availableCommands"].as<JsonArray>();
  String items[12];
  uint8_t n = 0;
  for (JsonVariant c : cmds) if (n < 12) items[n++] = c.as<String>();
  uiMenu(d["id"].as<String>(), items, n, cmdSel);
}

// Scene bonus: one key press -> whole-house "leaving home"
void runLeavingHomeScene() {
  uiScreen("Scene: Leaving home", "Sending commands...");
  uint8_t done = 0;
  for (uint8_t i = 0; i < devCount; i++) {
    JsonObject d = deviceAt(i);
    String want = "";
    for (JsonVariant c : d["availableCommands"].as<JsonArray>()) {
      String cmd = c.as<String>();
      if (cmd.equalsIgnoreCase("CLOSE") || cmd.equalsIgnoreCase("TURN OFF") ||
          cmd.equalsIgnoreCase("OFF")) { want = cmd; break; }
    }
    if (want.length()) {
      apiSendCommand(gToken, gHouseId, d["id"].as<String>(), want);
      done++;
      uiLine(LCD_ROWS - 1, String(done) + " devices done");
    }
  }
  showMessage("Scene finished", String(done) + " commands sent", ST_LOAD_DEVICES);
}

// ============================== WEB USER ACCOUNTS ===========================
// Accounts for the web remote, stored on the board itself (NVS key "webusers"
// holds {"user":"pass", ...}). The board issues in-RAM session tokens; every
// remote command must carry a valid session, so an unauthenticated browser
// can see the board is online but cannot control it.

#define WEB_MAX_SESSIONS 4
String webSessTok[WEB_MAX_SESSIONS];
String webSessUser[WEB_MAX_SESSIONS];
uint8_t webSessNext = 0;

bool webUserExists(const String &u) {
  JsonDocument d;
  deserializeJson(d, prefs.getString("webusers", "{}"));
  return !d[u].isNull();
}

bool webUserAdd(const String &u, const String &p) {
  JsonDocument d;
  deserializeJson(d, prefs.getString("webusers", "{}"));
  if (!d[u].isNull()) return false;
  d[u] = p;
  String out;
  serializeJson(d, out);
  prefs.putString("webusers", out);
  Serial.println("[users] registered: " + u);
  Serial.println("[users] store now: " + out);   // debug: local Serial only
  return true;
}

bool webUserCheck(const String &u, const String &p) {
  JsonDocument d;
  deserializeJson(d, prefs.getString("webusers", "{}"));
  return !d[u].isNull() && d[u].as<String>() == p && p.length() > 0;
}

String webNewSession(const String &u) {
  String tok = String(esp_random(), HEX) + String(esp_random(), HEX);
  webSessTok[webSessNext] = tok;          // ring buffer: oldest session drops
  webSessUser[webSessNext] = u;
  webSessNext = (webSessNext + 1) % WEB_MAX_SESSIONS;
  return tok;
}

bool webSessionValid(const String &tok) {
  if (tok.length() == 0) return false;
  for (uint8_t i = 0; i < WEB_MAX_SESSIONS; i++)
    if (webSessTok[i] == tok) return true;
  return false;
}

// ============================== MQTT REMOTE =================================
// Bridge for the web remote (webapp/remote.html). The Cirkit simulator only
// allows OUTBOUND connections, so the board keeps a link to a public MQTT
// broker; the web page talks to the same broker. The MQTT Last-Will makes the
// broker flip <prefix>/status to "offline" the instant the simulation stops,
// which is what locks the web remote when the board isn't running.

WiFiClient mqttSock;
PubSubClient mqtt(mqttSock);
unsigned long mqttLastTry = 0, mqttLastBeat = 0;

String mqttTopic(const char *leaf) { return String(MQTT_PREFIX) + "/" + leaf; }

void publishDevices() {
  if (!gToken.length() || gHouseId < 0) {
    mqtt.publish(mqttTopic("devices").c_str(), "{\"error\":\"not-authenticated\"}");
    return;
  }
  JsonDocument d;
  int code = apiGetDevices(gToken, gHouseId, d);
  if (code == 200) {
    String out;
    serializeJson(d, out);
    mqtt.publish(mqttTopic("devices").c_str(), out.c_str());
  } else {
    String err = String("{\"error\":\"http-") + code + "\"}";
    mqtt.publish(mqttTopic("devices").c_str(), err.c_str());
  }
}

void mqttCallback(char *t, byte *payload, unsigned int len) {
  JsonDocument d;
  if (deserializeJson(d, payload, len) != DeserializationError::Ok) return;
  String action = d["action"].as<String>();

  // ---- account actions (no session needed) ----
  if (action == "register" || action == "login") {
    String u = d["user"].as<String>(), p = d["pass"].as<String>();
    String reqId = d["reqId"].as<String>(), err = "", tok = "";
    bool ok = false;
    if (u.length() == 0 || p.length() == 0) err = "missing-fields";
    else if (action == "register") {
      if (webUserAdd(u, p)) ok = true; else err = "user-exists";
    } else {
      if (webUserCheck(u, p)) ok = true; else err = "bad-credentials";
    }
    if (ok) tok = webNewSession(u);
    String out = String("{\"reqId\":\"") + reqId + "\",\"ok\":" +
                 (ok ? "true" : "false") + ",\"user\":\"" + u +
                 "\",\"session\":\"" + tok + "\",\"error\":\"" + err + "\"}";
    mqtt.publish(mqttTopic("auth").c_str(), out.c_str());
    return;
  }

  // ---- everything else requires a valid board session ----
  if (!webSessionValid(d["session"].as<String>())) {
    mqtt.publish(mqttTopic("auth").c_str(), "{\"error\":\"invalid-session\"}");
    return;
  }
  if (action == "list") publishDevices();
  else if (action == "command" && gToken.length() && gHouseId >= 0) {
    String dev = d["deviceId"].as<String>(), cmd = d["command"].as<String>();
    int code = apiSendCommand(gToken, gHouseId, dev, cmd);
    String ack = String("{\"deviceId\":\"") + dev + "\",\"command\":\"" + cmd +
                 "\",\"http\":" + code + "}";
    mqtt.publish(mqttTopic("ack").c_str(), ack.c_str());
    publishDevices();
  }
}

void mqttTick() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    if (millis() - mqttLastTry < 5000) return;   // retry every 5 s, non-blocking
    mqttLastTry = millis();
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(4096);                    // device list JSON > 256 B default
    mqtt.setCallback(mqttCallback);
    String cid = String(MQTT_PREFIX) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    // Last-Will: broker announces "offline" for us if the sim dies
    if (mqtt.connect(cid.c_str(), mqttTopic("status").c_str(), 1, true, "offline")) {
      mqtt.publish(mqttTopic("status").c_str(), "online", true);
      mqtt.subscribe(mqttTopic("cmd").c_str());
    }
    return;
  }
  mqtt.loop();
  if (millis() - mqttLastBeat > 5000) {          // heartbeat for the web UI
    mqttLastBeat = millis();
    mqtt.publish(mqttTopic("beat").c_str(), String(millis() / 1000).c_str());
  }
}

// ============================== SETUP / LOOP ================================

void setup() {
  Serial.begin(115200);
  storageInit();
  // debug: dump the web-remote account store on every boot (Serial only —
  // never publish this over MQTT, the broker is public)
  Serial.println("[users] stored accounts: " + prefs.getString("webusers", "{}"));
  uiInit();
  keypadInit();
  uiScreen("PolyHome control", "Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  if (strlen(WIFI_PASSWORD)) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  else                       WiFi.begin(WIFI_SSID);   // open network
}

void loop() {
  mqttTick();   // keep the web remote alive in every state

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
    if (gToken.length()) {                    // stored token still valid?
      uiScreen("Checking token...");
      int code = apiGetHouses(gToken, gHouseId);
      if (code == 200 && gHouseId >= 0) {
        storageSaveHouseId(gHouseId);
        state = ST_LOAD_DEVICES;
        break;
      }
    }
    if (storageLoadAccount(gLogin, gPassword)) {  // auto re-login
      uiScreen("Logging in...", gLogin);
      int code = apiLogin(gLogin, gPassword, gToken);
      if (code == 200) { storageSaveToken(gToken); gRegistering = false; state = ST_SUBMIT; break; }
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
      entryReset(false);
      drawn = false;
      uiScreen(gRegistering ? "New account login:" : "Login:", "",
               "2-9 letters *del A=12", "# ok        D cancel");
      state = ST_ENTER_LOGIN;
    }
    break;
  }

  case ST_ENTER_LOGIN: {
    int e = entryUpdate();
    if (e == ENTRY_CHANGED) uiLine(1, entryDisplay());
    if (e == ENTRY_CANCELLED) state = ST_ACCOUNT_MENU;
    if (e == ENTRY_DONE && entryText.length() > 0) {
      gLogin = entryText;
      entryReset(false);
      uiScreen("Password:", "", "2-9 letters *del A=12", "# ok        D cancel");
      state = ST_ENTER_PASSWORD;
    }
    break;
  }

  case ST_ENTER_PASSWORD: {
    int e = entryUpdate();
    if (e == ENTRY_CHANGED) uiLine(1, entryDisplay());
    if (e == ENTRY_CANCELLED) state = ST_ACCOUNT_MENU;
    if (e == ENTRY_DONE && entryText.length() > 0) {
      gPassword = entryText;
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
      gToken = "";   // register returns no token -> must log in (required flow)
    }
    if (gToken.length() == 0) {
      uiScreen("Logging in...");
      int code = apiLogin(gLogin, gPassword, gToken);
      if (code != 200) { showMessage("Login failed", "HTTP " + String(code), ST_ACCOUNT_MENU); break; }
    }
    storageSaveAccount(gLogin, gPassword);
    storageSaveToken(gToken);                 // token persisted in NVS
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
    if (code == 403) { gToken = ""; state = ST_BOOT_AUTH; break; }  // silent re-login
    if (code != 200) { showMessage("Device list failed", "Open house in web?", ST_DEVICE_LIST); break; }
    devCount = gDevices["devices"].as<JsonArray>().size();
    if (devSel >= devCount) devSel = 0;
    if (devCount == 0) { showMessage("No devices found", "Open house in web!", ST_LOAD_DEVICES); break; }
    drawDeviceList();
    state = ST_DEVICE_LIST;
    break;
  }

  case ST_DEVICE_LIST: {
    char k = keypadGetKey();
    if (k == 'A' && devSel > 0)              { devSel--; drawDeviceList(); }
    if (k == 'B' && devSel + 1 < devCount)   { devSel++; drawDeviceList(); }
    if (k == '*') state = ST_LOAD_DEVICES;   // refresh states
    if (k == 'C') state = ST_RUN_SCENE;
    if (k == '#') { cmdSel = 0; drawCommandMenu(); state = ST_COMMAND_MENU; }
    break;
  }

  case ST_COMMAND_MENU: {
    char k = keypadGetKey();
    if (k == 'A' && cmdSel > 0)                  { cmdSel--; drawCommandMenu(); }
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
