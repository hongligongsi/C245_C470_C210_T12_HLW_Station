/**
 * @file encoder.h
 * @brief EC11 旋转编码器: 旋转 + 按键(短按/长按/连发现), 1ms 周期轮询
 */
#pragma once
#include <Arduino.h>

enum EncEvent {
  ENC_EVT_NONE = 0,
  ENC_EVT_CLICK,  // 短按释放
  ENC_EVT_HOLD,   // 长按达到阈值(仅一次)
  ENC_EVT_REPEAT, // 长按持续, 周期触发
};

class Encoder {
public:
  void begin();
  /// 每 1ms 调用
  void tick();

  /// 取出累计旋转步数(正/负), 取出后清零
  int16_t takeDelta();
  /// 取出一次按键事件
  EncEvent takeEvent();

  bool isPressed() const { return _pressed; }

private:
  bool _lastA = true;
  volatile int16_t _delta = 0;

  bool _pressed = false;
  uint8_t _stableCnt = 0;
  uint32_t _pressMs = 0;
  bool _holdFired = false;
  uint32_t _lastRepeatMs = 0;
  EncEvent _evt = ENC_EVT_NONE;

  uint32_t _lastStepMs = 0;
  uint8_t _speed = 1;

  // 按键版(-DINPUT_BUTTONS)专用状态: 加/减键消抖与连发
  bool _btnUp = false, _btnDn = false;
  uint8_t _btnUpCnt = 0, _btnDnCnt = 0;
  uint32_t _btnUpPressMs = 0, _btnDnPressMs = 0;
  uint32_t _btnUpRepMs = 0, _btnDnRepMs = 0;
  bool _btnUpRep = false, _btnDnRep = false;
};

extern Encoder Enc;
