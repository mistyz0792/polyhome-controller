#include "api_client.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Shared TLS client. setInsecure() skips certificate verification — acceptable
// for this prototype; a production device would pin the server certificate.
static WiFiClientSecure tls;
static bool tlsReady = false;

static void ensureTls() {
  if (!tlsReady) {
    tls.setInsecure();
    tlsReady = true;
  }
}

static int postJson(const String &url, const String &body,
                    const String &token, String &responseOut) {
  ensureTls();
  HTTPClient http;
  if (!http.begin(tls, url)) return -1;
  http.addHeader("Content-Type", "application/json");
  if (token.length()) http.addHeader("Authorization", "Bearer " + token);
  int code = http.POST(body);
  responseOut = (code > 0) ? http.getString() : String("");
  http.end();
  return code;
}

static int getJson(const String &url, const String &token, String &responseOut) {
  ensureTls();
  HTTPClient http;
  if (!http.begin(tls, url)) return -1;
  if (token.length()) http.addHeader("Authorization", "Bearer " + token);
  int code = http.GET();
  responseOut = (code > 0) ? http.getString() : String("");
  http.end();
  return code;
}

static String credentialsBody(const String &login, const String &password) {
  JsonDocument doc;
  doc["login"] = login;
  doc["password"] = password;
  String body;
  serializeJson(doc, body);
  return body;
}

int apiRegister(const String &login, const String &password) {
  String resp;
  return postJson(String(API_BASE) + "/users/register",
                  credentialsBody(login, password), "", resp);
}

int apiLogin(const String &login, const String &password, String &tokenOut) {
  String resp;
  int code = postJson(String(API_BASE) + "/users/auth",
                      credentialsBody(login, password), "", resp);
  tokenOut = "";
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok)
      tokenOut = doc["token"].as<String>();
    if (tokenOut.length() == 0) return -1;
  }
  return code;
}

int apiGetHouses(const String &token, int &houseIdOut) {
  String resp;
  int code = getJson(String(API_BASE) + "/houses", token, resp);
  houseIdOut = -1;
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      for (JsonObject h : doc.as<JsonArray>()) {
        if (h["owner"].as<bool>()) { houseIdOut = h["houseId"].as<int>(); break; }
      }
      // Fallback: user may only be a guest of someone else's house.
      if (houseIdOut < 0 && doc.as<JsonArray>().size() > 0)
        houseIdOut = doc.as<JsonArray>()[0]["houseId"].as<int>();
    }
  }
  return code;
}

int apiGetDevices(const String &token, int houseId, JsonDocument &out) {
  String resp;
  int code = getJson(String(API_BASE) + "/houses/" + houseId + "/devices",
                     token, resp);
  if (code == 200) {
    if (deserializeJson(out, resp) != DeserializationError::Ok) return -1;
  }
  return code;
}

int apiSendCommand(const String &token, int houseId,
                   const String &deviceId, const String &command) {
  JsonDocument doc;
  doc["command"] = command;
  String body, resp;
  serializeJson(doc, body);
  return postJson(String(API_BASE) + "/houses/" + houseId +
                  "/devices/" + deviceId + "/command", body, token, resp);
}
