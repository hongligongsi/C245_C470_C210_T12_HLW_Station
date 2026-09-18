#include "encoder.h"
#include "board.h"

// 全局单例定义
Encoder Enc;

// 若实际旋转方向与调温方向相反, 在 platformio.ini 加 -DENC_INVERTED
#ifdef ENC_INVERTED
#define ENC_SIGN (-1)
#else
#define ENC_SIGN (1)
#endif

#define BTN_DEBOUNCE_MS 20
#define BTN_HOLD_MS 800
#define BTN_REPEAT_MS 180

void Encoder::begin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_C, INPUT_PULLUP);
  _lastA = digitalRead(PIN_ENC_A) ? true : false;
}

void Encoder::tick() {
  // ---- 旋转 (A 相边沿 + B 相判向, 板载 RC 滤波已完成硬件消抖) ----
  bool a = digitalRead(PIN_ENC_A) ? true : false;
  bool b = digitalRead(PIN_ENC_B) ? true : false;
  if (a != _lastA) {
    // 同相为一个方向, 异相为另一方向; 每个节拍(A 边沿)计一步
    int8_t step = (a == b) ? 1 : -1;
    step *= ENC_SIGN;

    // 加速: 连续快转时每步权重 1..4
    uint32_t now = millis();
    if (now - _lastStepMs < 70)
      _speed = (uint8_t)min((int)_speed + 1, 4);
    else
      _speed = 1;
    _lastStepMs = now;

    _delta += (int16_t)(step * _speed);
    _lastA = a;
  }

  // ---- 按键 (带消抖) ----
  bool raw = digitalRead(PIN_ENC_C) == LOW;
  if (raw != _pressed) {
    if (++_stableCnt >= BTN_DEBOUNCE_MS) {
      _pressed = raw;
      _stableCnt = 0;
      if (_pressed) {
        _pressMs = millis();
        _holdFired = false;
        _lastRepeatMs = _pressMs;
      } else {
        // 释放
        if (!_holdFired)
          _evt = ENC_EVT_CLICK;
      }
    }
  } else {
    _stableCnt = 0;
  }

  if (_pressed) {
    uint32_t held = millis() - _pressMs;
    if (!_holdFired && held >= BTN_HOLD_MS) {
      _holdFired = true;
      _evt = ENC_EVT_HOLD;
      _lastRepeatMs = millis();
    } else if (_holdFired && millis() - _lastRepeatMs >= BTN_REPEAT_MS) {
      _lastRepeatMs = millis();
      _evt = ENC_EVT_REPEAT;
    }
  }
}

int16_t Encoder::takeDelta() {
  int16_t d = _delta;
  _delta = 0;
  return d;
}

EncEvent Encoder::takeEvent() {
  EncEvent e = _evt;
  _evt = ENC_EVT_NONE;
  return e;
}
