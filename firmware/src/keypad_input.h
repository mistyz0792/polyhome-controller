#pragma once
#include <Arduino.h>

// 4x4 keypad handling: raw non-blocking key reads + a multi-tap text editor
// (press '2' repeatedly -> a, b, c, 2) for entering login/password on keys.
//
// Editor keys:  2-9 letters+digit (multi-tap), 0 -> space/0, 1 -> 1
//               '*' backspace, '#' validate, 'D' cancel, 'A' toggle abc/123

void keypadInit();
char keypadGetKey();   // returns pressed key or 0; non-blocking, debounced

enum EntryEvent {
  ENTRY_IDLE,      // nothing happened
  ENTRY_CHANGED,   // text (or cycling char) changed -> redraw
  ENTRY_DONE,      // '#' pressed
  ENTRY_CANCELLED  // 'D' pressed
};

class MultiTapEntry {
public:
  void reset(bool digitsOnly = false);
  EntryEvent update();            // call every loop()
  String textWithCursor() const;  // committed text + currently-cycling char
  const String &value() const { return _text; }

private:
  void commitPending();
  String _text;
  char _pendingKey = 0;    // key currently being cycled
  uint8_t _tapIndex = 0;
  unsigned long _lastTap = 0;
  bool _digitsOnly = false;
};
