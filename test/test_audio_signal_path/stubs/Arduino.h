#pragma once

class HardwareSerial {
 public:
  template <typename... Args>
  int printf(const char*, Args...) {
    return 0;
  }
};

extern HardwareSerial Serial;
