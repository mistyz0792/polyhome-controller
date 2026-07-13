#include "storage.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NS = "polyhome";

void storageInit() {
  prefs.begin(NS, false);
}

void storageSaveAccount(const String &login, const String &password) {
  prefs.putString("login", login);
  prefs.putString("password", password);
}

bool storageLoadAccount(String &login, String &password) {
  login = prefs.getString("login", "");
  password = prefs.getString("password", "");
  return login.length() > 0 && password.length() > 0;
}

void storageSaveToken(const String &token) {
  prefs.putString("token", token);
}

String storageLoadToken() {
  return prefs.getString("token", "");
}

void storageSaveHouseId(int houseId) {
  prefs.putInt("houseId", houseId);
}

int storageLoadHouseId() {
  return prefs.getInt("houseId", -1);
}

void storageClearAll() {
  prefs.clear();
}
