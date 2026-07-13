#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// All HTTP calls to the PolyHome API. Every function returns the HTTP status
// code (or -1 on connection failure) so the UI can react (e.g. 403 -> re-login,
// 409 -> login already taken).

// POST /users/register — NOTE: returns no token; call apiLogin() afterwards.
int apiRegister(const String &login, const String &password);

// POST /users/auth — the only endpoint that returns a token.
int apiLogin(const String &login, const String &password, String &tokenOut);

// GET /houses — picks the house owned by this user (owner == true).
int apiGetHouses(const String &token, int &houseIdOut);

// GET /houses/<id>/devices — requires the house page to be open in a browser.
// The document keeps: devices[i] { id, type, availableCommands[], opening|power }
int apiGetDevices(const String &token, int houseId, JsonDocument &out);

// POST /houses/<id>/devices/<devId>/command — command must come from that
// device's availableCommands list.
int apiSendCommand(const String &token, int houseId,
                   const String &deviceId, const String &command);
