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

#ifdef INPUT_BUTTONS

// ---------------------------------------------------------------------------
// 按键版固件 (-DINPUT_BUTTONS): 加/减/确认 三键, 对外接口与编码器版完全一致
// 默认接线与 EC11 同座: A脚=加 B脚=减 C脚=确认,
// 可用 -DBTN_UP_PIN= -DBTN_DOWN_PIN= -DBTN_OK_PIN= 覆盖
// ---------------------------------------------------------------------------
#ifndef BTN_UP_PIN
#define BTN_UP_PIN PIN_ENC_A
#endif
#ifndef BTN_DOWN_PIN
#define BTN_DOWN_PIN PIN_ENC_B
#endif
#ifndef BTN_OK_PIN
#define BTN_OK_PIN PIN_ENC_C
#endif

#define BTN_REPEAT_DELAY_MS 350 // 首次连发延时
#define BTN_REPEAT_PERIOD_MS 100

void Encoder::begin() {
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_OK_PIN, INPUT_PULLUP);
}

void Encoder::tick() {
  uint32_t now = millis();

  // ---- 加/减键: 按下即一步, 长按连发(模拟编码器旋转) ----
  bool up = digitalRead(BTN_UP_PIN) == LOW;
  if (up != _btnUp) {
    if (++_btnUpCnt >= BTN_DEBOUNCE_MS) {
      _btnUp = up;
      _btnUpCnt = 0;
      if (_btnUp) {
        _delta += 1;
        _btnUpPressMs = now;
        _btnUpRep = false;
        _btnUpRepMs = now;
      }
    }
  } else {
    _btnUpCnt = 0;
  }
  if (_btnUp) {
    if (!_btnUpRep && now - _btnUpPressMs >= BTN_REPEAT_DELAY_MS) {
      _btnUpRep = true;
      _btnUpRepMs = now;
      _delta += 1;
    } else if (_btnUpRep && now - _btnUpRepMs >= BTN_REPEAT_PERIOD_MS) {
      _btnUpRepMs = now;
      _delta += 1;
    }
  }

  bool dn = digitalRead(BTN_DOWN_PIN) == LOW;
  if (dn != _btnDn) {
    if (++_btnDnCnt >= BTN_DEBOUNCE_MS) {
      _btnDn = dn;
      _btnDnCnt = 0;
      if (_btnDn) {
        _delta -= 1;
        _btnDnPressMs = now;
        _btnDnRep = false;
        _btnDnRepMs = now;
      }
    }
  } else {
    _btnDnCnt = 0;
  }
  if (_btnDn) {
    if (!_btnDnRep && now - _btnDnPressMs >= BTN_REPEAT_DELAY_MS) {
      _btnDnRep = true;
      _btnDnRepMs = now;
      _delta -= 1;
    } else if (_btnDnRep && now - _btnDnRepMs >= BTN_REPEAT_PERIOD_MS) {
      _btnDnRepMs = now;
      _delta -= 1;
    }
  }

  // ---- 确认键: 短按=CLICK, 长按=HOLD(一次)+REPEAT(连发), 与编码器同语义 ----
  bool raw = digitalRead(BTN_OK_PIN) == LOW;
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

#else // 旋转编码器版(默认)

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

#endif

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
