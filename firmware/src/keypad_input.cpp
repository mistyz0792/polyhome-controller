#include "keypad_input.h"
#include "config.h"

// ---- raw matrix scan (non-blocking, millis()-based debounce) ----

static const char KEYMAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
static const uint8_t ROWS[4] = {KP_R1, KP_R2, KP_R3, KP_R4};
static const uint8_t COLS[4] = {KP_C1, KP_C2, KP_C3, KP_C4};

static char lastKey = 0;
static unsigned long lastEdge = 0;
static const unsigned long DEBOUNCE_MS = 30;

void keypadInit() {
  for (uint8_t r = 0; r < 4; r++) {
    pinMode(ROWS[r], OUTPUT);
    digitalWrite(ROWS[r], HIGH);
  }
  for (uint8_t c = 0; c < 4; c++) pinMode(COLS[c], INPUT_PULLUP);
}

static char scanOnce() {
  char found = 0;
  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(ROWS[r], LOW);
    for (uint8_t c = 0; c < 4; c++)
      if (digitalRead(COLS[c]) == LOW) found = KEYMAP[r][c];
    digitalWrite(ROWS[r], HIGH);
  }
  return found;
}

char keypadGetKey() {
  char now = scanOnce();
  unsigned long t = millis();
  if (now != lastKey) {
    if (t - lastEdge < DEBOUNCE_MS) return 0;
    lastEdge = t;
    lastKey = now;
    if (now != 0) return now;   // report on press edge only
  }
  return 0;
}

// ---- multi-tap text editor ----

static const char *TAPS[10] = {
  "0 ",    "1",     "abc2", "def3", "ghi4",
  "jkl5",  "mno6",  "pqrs7","tuv8", "wxyz9"
};
static const unsigned long TAP_TIMEOUT_MS = 900;

void MultiTapEntry::reset(bool digitsOnly) {
  _text = "";
  _pendingKey = 0;
  _tapIndex = 0;
  _lastTap = 0;
  _digitsOnly = digitsOnly;
}

void MultiTapEntry::commitPending() {
  if (_pendingKey) {
    const char *set = TAPS[_pendingKey - '0'];
    _text += set[_tapIndex % strlen(set)];
    _pendingKey = 0;
    _tapIndex = 0;
  }
}

EntryEvent MultiTapEntry::update() {
  // auto-commit the cycling char after the timeout
  if (_pendingKey && millis() - _lastTap > TAP_TIMEOUT_MS) {
    commitPending();
    return ENTRY_CHANGED;
  }

  char k = keypadGetKey();
  if (!k) return ENTRY_IDLE;

  if (k == '#') { commitPending(); return ENTRY_DONE; }
  if (k == 'D') { return ENTRY_CANCELLED; }
  if (k == '*') {                       // backspace
    if (_pendingKey) { _pendingKey = 0; _tapIndex = 0; }
    else if (_text.length()) _text.remove(_text.length() - 1);
    return ENTRY_CHANGED;
  }
  if (k == 'A') {                       // toggle letters/digits
    commitPending();
    _digitsOnly = !_digitsOnly;
    return ENTRY_CHANGED;
  }
  if (k < '0' || k > '9') return ENTRY_IDLE;  // B/C unused here

  if (_digitsOnly) { commitPending(); _text += k; return ENTRY_CHANGED; }

  if (k == _pendingKey) {               // next char in the same key's cycle
    _tapIndex++;
  } else {                              // new key: commit previous, start cycle
    commitPending();
    _pendingKey = k;
    _tapIndex = 0;
  }
  _lastTap = millis();
  return ENTRY_CHANGED;
}

String MultiTapEntry::textWithCursor() const {
  String s = _text;
  if (_pendingKey) {
    const char *set = TAPS[_pendingKey - '0'];
    s += set[_tapIndex % strlen(set)];
  }
  return s;
}
