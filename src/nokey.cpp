/**
 * @file nokey.cpp
 * @brief 无键交互实现: 手柄拔插手势 + 无键设置向导(见 nokey.h)
 */
#include "nokey.h"
#include "board.h"
#include "buzzer.h"
#include "font5x7.h"
#include "settings.h"
#include "station.h"
#include "tft.h"
#include "ui.h"
#include <stdio.h>

Keyless NoKey;

#ifdef NO_INPUT

// ---------------------------------------------------------------------------
// 手势时间参数
// ---------------------------------------------------------------------------
// 注: 拔出要 500ms 消抖才被识别, 所以"快插快拔"实测约 600~800ms,
//     判定窗口放到 900ms; 保持 1.4s 以上才算"减", 两者不重叠
#define NK_TAP_MAX_MS 900    // 插入短于此时长 = "快插快拔"(加一步)
#define NK_HOLD_MS 1400      // 插入保持超过此时长 = "减一步"
#define NK_ADVANCE_MS 3000   // 停手 3s -> 保存并进入下一项
#define NK_ENTER_GAP_MS 6000 // 连插手势总窗口
#define NK_ENTER_COUNT 3     // 连插几次进入设置
#define NK_DONE_MS 1500      // "已保存" 停留时间

// 设置项(顺序即向导顺序)
enum {
  NK_BOOT_TEMP = 0,  // 开机温度(= 快捷温度第二档, 上电默认加热到该温度)
  NK_STANDBY_TEMP,   // 待机温度
  NK_STANDBY_MIN,    // 待机时间
  NK_SLEEP_MIN,      // 休眠时间
  NK_POWER,          // 功率上限
  NK_ITEM_COUNT
};

struct NkItem {
  const char *label;
  int16_t step;
};

static const NkItem NK[NK_ITEM_COUNT] = {
    {"开机温度", 10},
    {"待机温度", 5},
    {"待机时间", 1},
    {"休眠时间", 5},
    {"功率上限", 5},
};

static void beep(uint16_t ms) {
  if (Cfg.soundOn)
    Buzz.beep(ms);
}

static int16_t nkGet(uint8_t i) {
  switch (i) {
  case NK_BOOT_TEMP:
    return Cfg.quickTemp[1];
  case NK_STANDBY_TEMP:
    return Cfg.standbyTemp;
  case NK_STANDBY_MIN:
    return (int16_t)Cfg.standbyTimeMin;
  case NK_SLEEP_MIN:
    return (int16_t)Cfg.sleepTimeMin;
  default:
    return (int16_t)Cfg.powerLimitPct;
  }
}

static int16_t nkMin(uint8_t i) {
  switch (i) {
  case NK_STANDBY_TEMP:
    return 50;
  case NK_STANDBY_MIN:
    return 0;
  case NK_SLEEP_MIN:
    return 5;
  case NK_POWER:
    return 10;
  default:
    return Cfg.tempMin;
  }
}

static int16_t nkMax(uint8_t i) {
  switch (i) {
  case NK_BOOT_TEMP:
    return (int16_t)Stn.tempMaxEff();
  case NK_STANDBY_TEMP:
    return 300;
  case NK_STANDBY_MIN:
    return 30;
  case NK_SLEEP_MIN:
    return 60;
  default:
    return 100;
  }
}

static void nkSet(uint8_t i, int16_t v) {
  switch (i) {
  case NK_BOOT_TEMP:
    Cfg.quickTemp[1] = v;
    Stn.setSetTemp(v); // 立即按新温度控温
    break;
  case NK_STANDBY_TEMP:
    Cfg.standbyTemp = v;
    break;
  case NK_STANDBY_MIN:
    Cfg.standbyTimeMin = (uint8_t)v;
    break;
  case NK_SLEEP_MIN:
    Cfg.sleepTimeMin = (uint8_t)v;
    break;
  default:
    Cfg.powerLimitPct = (uint8_t)v;
    break;
  }
  Settings_Save();
  Stn.syncConfig();
}

// ---------------------------------------------------------------------------
void Keyless::begin() {
  _lastTip = Stn.tipPresent();
  _gestMs = millis();
  _tapCount = 0;
}

void Keyless::enter() {
  _active = true;
  _finished = false;
  _needClear = true;
  _item = 0;
  _base = nkGet(0);
  _count = 0;
  _lastActMs = millis();
  _tapCount = 0;
  Stn.setConfigLock(true); // 设置期间强制停热(插着手柄也不会烧红)
  beep(60);
}

void Keyless::commit() {
  int16_t v = preview();
  if (v != _base || _item == NK_BOOT_TEMP)
    nkSet(_item, v);
  _base = v;
}

void Keyless::finish() {
  _finished = true;
  _doneMs = millis();
  _needClear = true;
  Stn.setConfigLock(false);
  beep(80);
}

// 预览值: 原值 + 步数×步进, 超限按步进环绕(保证加减都能到每一档)
int16_t Keyless::preview() const {
  int16_t lo = nkMin(_item), hi = nkMax(_item);
  int16_t st = NK[_item].step;
  int32_t span = (hi - lo) / st + 1; // 档位数量
  if (span <= 0)
    return _base;
  int32_t k = ((int32_t)_base - lo) / st + _count;
  k %= span;
  if (k < 0)
    k += span;
  return (int16_t)(lo + k * st);
}

void Keyless::tick() {
  uint32_t now = millis();
  bool tip = Stn.tipPresent();

  // -------------------------------------------------------------------------
  // 拔插边沿
  // -------------------------------------------------------------------------
  if (tip != _lastTip) {
    _lastTip = tip;
    if (tip) {
      // ---- 插入 ----
      _insertMs = now;
      _holdFired = false;

      // 任何插入都算一次"用户在操作": 重置待机计时, 免得设置时被判待机
      Stn.notifyInput();

      if (!_active) {
        if (Stn.mode() == MODE_FAULT) {
          Stn.clearFault(); // 无键版: 拔插一次清故障
          beep(40);
          _tapCount = 0;
        } else if (Stn.mode() == MODE_SLEEP) {
          Stn.wakeFromSleep(); // 无键版: 拔插一次唤醒
          beep(40);
          _tapCount = 0;
        }
      }
    } else {
      // ---- 拔出 ----
      uint32_t dur = now - _insertMs;

      if (_active && !_finished) {
        if (!_holdFired && dur <= NK_TAP_MAX_MS) {
          _count++; // 快插快拔 = +1
          _lastActMs = now;
          beep(8);
        }
      } else if (!_active) {
        // 连插计数: 只有"插一下就拔"才算手势, 正常插着使用会清零
        if (dur > NK_TAP_MAX_MS) {
          _tapCount = 0;
        } else {
          if (_tapCount == 0 || now - _gestMs > NK_ENTER_GAP_MS) {
            _tapCount = 0;
            _gestMs = now;
          }
          if (++_tapCount >= NK_ENTER_COUNT)
            enter();
        }
      }
    }
  }

  if (!_active)
    return;

  // -------------------------------------------------------------------------
  // 完成提示: 停留 1.5s 后退出
  // -------------------------------------------------------------------------
  if (_finished) {
    if (now - _doneMs > NK_DONE_MS) {
      _active = false;
      _finished = false;
      Screen.invalidate();
    }
    return;
  }

  // -------------------------------------------------------------------------
  // 插入保持 = -1 步
  // -------------------------------------------------------------------------
  if (tip && !_holdFired && now - _insertMs >= NK_HOLD_MS) {
    _holdFired = true;
    _count--;
    _lastActMs = now;
    beep(25);
  }

  // -------------------------------------------------------------------------
  // 停手 3s: 保存本项并进入下一项
  // -------------------------------------------------------------------------
  if (now - _lastActMs >= NK_ADVANCE_MS) {
    commit();
    if (_item + 1 >= NK_ITEM_COUNT) {
      finish();
    } else {
      _item++;
      _base = nkGet(_item);
      _count = 0;
      _lastActMs = now;
      _needClear = true; // 换项: 整页重画(项名/上下限都变了)
      beep(20);
    }
  }
}

// ---------------------------------------------------------------------------
// 无键设置页
// ---------------------------------------------------------------------------
void Keyless::render() {
  if (!_active)
    return;

  const bool wide = (LCD_W >= 200);

  if (_needClear) {
    Lcd.clear(Pal.bg);
    _needClear = false;
    _lastItem = 0xFF;
    _lastVal = -32000;
    _lastBarW = -1;
  }

  // ---- 完成提示 ----
  if (_finished) {
    if (_lastItem == 0xFE)
      return;
    _lastItem = 0xFE;
    const uint8_t ts = wide ? 4 : 2;
    const char *t = "已保存";
    int16_t w = Lcd.textWidth(t, ts, ts);
    Lcd.drawText((LCD_W - w) / 2, LCD_H / 2 - 12 * ts / 2, t, Pal.ok, Pal.bg, ts,
                 ts);
    return;
  }

  // ---- 标题栏 ----
  if (_lastItem == 0xFF) {
    const int16_t hdr = 18;
    Lcd.fillRect(0, 0, LCD_W, hdr, Pal.panel);
    Lcd.drawText(4, 3, "无键设置", Pal.fg, Pal.panel, 1);
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", (unsigned)(_item + 1),
             (unsigned)NK_ITEM_COUNT);
    Lcd.drawText(LCD_W - Lcd.textWidth(pg, 1) - 4, 3, pg, Pal.accent, Pal.panel,
                 1);
    Lcd.drawHLine(0, hdr, LCD_W, Pal.grid);
  }

  // ---- 项名 ----
  if (_lastItem != _item) {
    _lastItem = _item;
    const uint8_t ls = wide ? 2 : 1; // 汉字 24px / 12px
    int16_t y = wide ? 22 : 20;
    int16_t w = Lcd.textWidth(NK[_item].label, 1, ls);
    Lcd.fillRect(0, y - 2, LCD_W, 12 * ls + 4, Pal.bg);
    Lcd.drawText((LCD_W - w) / 2, y, NK[_item].label, Pal.fg, Pal.bg, 1, ls);
    _lastVal = -32000;
  }

  // ---- 数值 ----
  int16_t v = preview();
  if (v != _lastVal) {
    _lastVal = v;
    char buf[16];
    if (_item == NK_STANDBY_MIN || _item == NK_SLEEP_MIN)
      snprintf(buf, sizeof(buf), "%d分", (int)v);
    else if (_item == NK_POWER)
      snprintf(buf, sizeof(buf), "%d%%", (int)v);
    else
      snprintf(buf, sizeof(buf), "%d%cC", (int)v, (char)FONT_DEG);

    const uint8_t vs = wide ? 8 : 4; // ASCII 56px / 28px
    const int16_t vh = 7 * vs;
    const int16_t vy = wide ? 52 : 36;
    int16_t w = Lcd.textWidth(buf, vs, 1);
    Lcd.fillRect(0, vy - 2, LCD_W, vh + 4, Pal.bg);
    Lcd.drawText((LCD_W - w) / 2, vy, buf, Pal.accent, Pal.bg, vs, 1);
  }

  // ---- 操作提示(三项, 静态) ----
  if (_lastBarW < 0) {
    const int16_t hy = wide ? 120 : 74;
    const int16_t lh = wide ? 18 : 14;
    char s1[24], s2[24];
    snprintf(s1, sizeof(s1), "快插快拔 +%d", (int)NK[_item].step);
    snprintf(s2, sizeof(s2), "插入保持 -%d", (int)NK[_item].step);
    const char *lines[3] = {s1, s2, "停手3秒 下一项"};
    for (uint8_t i = 0; i < 3; i++) {
      int16_t w = Lcd.textWidth(lines[i], 1);
      Lcd.drawText((LCD_W - w) / 2, hy + i * lh, lines[i],
                   (i == 0) ? Pal.ok : ((i == 1) ? Pal.warn : Pal.grid), Pal.bg,
                   1);
    }
  }

  // ---- 3 秒倒计时进度条 ----
  int16_t barW = (int16_t)((LCD_W - 20) *
                          (int32_t)(NK_ADVANCE_MS - (millis() - _lastActMs)) /
                          NK_ADVANCE_MS);
  if (barW < 0)
    barW = 0;
  if (barW != _lastBarW) {
    _lastBarW = barW;
    const int16_t by = LCD_H - 16;
    Lcd.fillRect(8, by, LCD_W - 16, 6, Pal.gridMin);
    Lcd.fillRect(8, by, barW, 6, Pal.accent);
  }
}

#else // 有输入设备(编码器/三键): 不启用无键交互

void Keyless::begin() {}
void Keyless::tick() {}
void Keyless::render() {}
int16_t Keyless::preview() const { return 0; }
void Keyless::commit() {}
void Keyless::enter() {}
void Keyless::finish() {}

#endif
