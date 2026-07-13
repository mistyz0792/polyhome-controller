#pragma once
#include <Arduino.h>

// NVS-backed persistent storage (survives reboot & power loss).
// Required by the assignment: the auth token must be stored on the device.

void storageInit();

void storageSaveAccount(const String &login, const String &password);
bool storageLoadAccount(String &login, String &password);

void storageSaveToken(const String &token);
String storageLoadToken();

void storageSaveHouseId(int houseId);
int  storageLoadHouseId();   // -1 if none

void storageClearAll();
