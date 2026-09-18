/**
 * @file buzzer.h
 * @brief 无源蜂鸣器非阻塞驱动 (2.7kHz), 短提示音
 */
#pragma once
#include <Arduino.h>

class Buzzer {
public:
  void begin();
  void tickMicros();      // 主循环内尽可能频繁调用
  void beep(uint16_t ms); // 提示音
  bool busy() const { return _remainMs > 0; }

private:
  int16_t _remainMs = 0;
  uint32_t _lastToggleUs = 0;
  bool _out = false;
};

extern Buzzer Buzz;
