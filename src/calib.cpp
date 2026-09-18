/**
 * @file calib.cpp
 * @brief 全屏可视化校准页实现: 实时 V/I/P/T + 曲线 + < 值 > 旋钮编辑
 *        所有实时数据直接读取 Stn 单例(采样唯一来源), 本页不另启 ADC
 */
#include "calib.h"
#include "board.h"
#include "font5x7.h"
#include "settings.h"
#include "station.h"
#include "tft.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

CalibPage Calib;

// 宽屏布局
#if LCD_W >= 200
#define C_HDR_H 20
#define C_BOX_X 4
#define C_BOX_Y 26
#define C_BOX_W 148
#define C_BOX_H 92
#define C_VAL_Y 58
#define C_CUR_X0 26
#define C_CUR_X1 294
#define C_CUR_Y0 122
#define C_CUR_Y1 220
#define C_TMAX 480
#else
// 小屏(80x160 / 135x240)
#define C_HDR_H 10
#define C_CUR_X0 2
#define C_CUR_X1 (LCD_W - 3)
#define C_CUR_Y0 72
#define C_CUR_Y1 (LCD_H - 4)
#define C_TMAX 480
#endif

void CalibPage::enter(const MenuItem *it) {
  _item = it;
  _ef = (it && it->type == NT_FLOAT) ? *(const float *)it->ptr : 0.0f;
  _head = 0;
  memset(_powHist, 0, sizeof(_powHist));
  memset(_tempHist, 0, sizeof(_tempHist));
  _needsClear = true;
  _v10 = _c100 = _p10 = -1;
  _t1 = -2;
  _valCache[0] = 0;
}

void CalibPage::exit() {
  _item = nullptr;
  _needsClear = true;
}

void CalibPage::move(int16_t delta) {
  if (!_item || delta == 0)
    return;
  _ef += (float)delta * _item->fstep;
  if (_ef < _item->fmin)
    _ef = _item->fmin;
  if (_ef > _item->fmax)
    _ef = _item->fmax;
}

void CalibPage::select() {
  if (!_item)
    return;
  *(float *)_item->ptr = _ef;
  Settings_Save();
  Stn.syncConfig();
  exit();
}

void CalibPage::back() { exit(); }

void CalibPage::pushHistory() {
  _powHist[_head] = (uint8_t)((uint32_t)Stn.duty() * 100 / PWM_MAX);
  int16_t t = (int16_t)(Stn.tipTemp() + 0.5f);
  if (t < 0)
    t = 0;
  if (t > 500)
    t = 500;
  _tempHist[_head] = t;
  _head = (_head + 1) % HN;
}

// 实时数据行: "V 23.9V" 蓝 / "A 0.02A" 黄 / "P 0.2W" 紫 / "T 300C" 红
#if LCD_W >= 200
static void drawLiveRow(int16_t x, int16_t y, const char *tag,
                        const char *value, uint16_t col) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%s %s", tag, value);
  Lcd.drawText(x, y, buf, col, Pal.bg, 1);
}
#endif

void CalibPage::render() {
  if (!_item)
    return;

#if LCD_W >= 200
  // ---- 首帧: 静态框架 ----
  if (_needsClear) {
    // 顶栏(反色)
    Lcd.fillRect(0, 0, LCD_W, C_HDR_H, Pal.fg);
    Lcd.drawText(3, (C_HDR_H - 14) / 2, _item->label, Pal.bg, Pal.fg, 2);
    const char *hint = "CLICK=SAVE  HOLD=EXIT";
    int16_t hw = Lcd.textWidth(hint, 1);
    Lcd.drawText(LCD_W - hw - 4, C_HDR_H / 2 - 3, hint, Pal.bg, Pal.fg, 1);

    // 实时数据盒
    Lcd.drawRect(C_BOX_X, C_BOX_Y, C_BOX_W, C_BOX_H, Pal.grid);
    Lcd.drawText(C_BOX_X + 4, C_BOX_Y - 5, "LIVE", Pal.grid, Pal.bg, 1);

    // 值编辑: <  value  >
    Lcd.drawText(164, C_VAL_Y + 8, "<", Pal.warn, Pal.bg, 3);
    Lcd.drawText(300, C_VAL_Y + 8, ">", Pal.warn, Pal.bg, 3);
    const char *vh = "ADJUST";
    Lcd.drawText((320 - Lcd.textWidth(vh, 1)) / 2 + 20, 42, vh, Pal.grid,
                 Pal.bg, 1);

    // 曲线区背景(网格/刻度在曲线刷新时重画)
    Lcd.fillRect(C_CUR_X0, C_CUR_Y0, C_CUR_X1 - C_CUR_X0 + 1,
                 C_CUR_Y1 - C_CUR_Y0 + 1, Pal.bg);
    _needsClear = false;
  }

  // ---- 实时数据(仅变化行重画) ----
  char buf[16];
  int16_t v10 = (int16_t)(Stn.vbus() * 10.0f);
  if (v10 != _v10) {
    Lcd.fillRect(C_BOX_X + 2, 32, C_BOX_W - 4, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "%.1f V", Stn.vbus());
    drawLiveRow(C_BOX_X + 6, 33, "V", buf, COL_BLUE);
    _v10 = v10;
  }
  int16_t c100 = (int16_t)(Stn.current() * 100.0f);
  if (c100 != _c100) {
    Lcd.fillRect(C_BOX_X + 2, 50, C_BOX_W - 4, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "%.2f A", Stn.current());
    drawLiveRow(C_BOX_X + 6, 51, "A", buf, COL_YELLOW);
    _c100 = c100;
  }
  int16_t p10 = (int16_t)(Stn.power() * 10.0f);
  if (p10 != _p10) {
    Lcd.fillRect(C_BOX_X + 2, 68, C_BOX_W - 4, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "%.1f W", Stn.power());
    drawLiveRow(C_BOX_X + 6, 69, "P", buf, COL_MAGENTA);
    _p10 = p10;
  }
  int16_t t1 = Stn.tipPresent() ? (int16_t)(Stn.tipTemp() + 0.5f) : -1;
  if (t1 != _t1 || Stn.tipPresent() != _tValid) {
    Lcd.fillRect(C_BOX_X + 2, 86, C_BOX_W - 4, 9, Pal.bg);
    if (Stn.tipPresent()) {
      snprintf(buf, sizeof(buf), "%d C", (int)t1);
      drawLiveRow(C_BOX_X + 6, 87, "T", buf, Pal.danger);
    } else {
      drawLiveRow(C_BOX_X + 6, 87, "T", "---", Pal.danger);
    }
    _t1 = t1;
    _tValid = Stn.tipPresent();
  }

  // ---- 编辑值(暂存, 旋转即变) ----
  char val[16];
  if (_item->suffix)
    snprintf(val, sizeof(val), "%.1f%s", _ef, _item->suffix);
  else
    snprintf(val, sizeof(val), "%.1f", _ef);
  if (strcmp(val, _valCache) != 0) {
    Lcd.fillRect(186, C_VAL_Y, 110, 24, Pal.bg);
    int16_t w = Lcd.textWidth(val, 3);
    Lcd.drawText(186 + (110 - w) / 2, C_VAL_Y + 4, val, Pal.accent, Pal.bg, 3);
    strncpy(_valCache, val, sizeof(_valCache) - 1);
    _valCache[sizeof(_valCache) - 1] = 0;
  }

#else
  // ============ 小屏 ============
  if (_needsClear) {
    Lcd.fillRect(0, 0, LCD_W, C_HDR_H, Pal.fg);
    Lcd.drawText(2, 2, _item->label, Pal.bg, Pal.fg, 1);
    _needsClear = false;
  }

  char buf[16];
  int16_t v10 = (int16_t)(Stn.vbus() * 10.0f);
  if (v10 != _v10) {
    Lcd.fillRect(0, 12, LCD_W, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "V %.1f", Stn.vbus());
    Lcd.drawText(2, 12, buf, COL_BLUE, Pal.bg, 1);
    _v10 = v10;
  }
  int16_t c100 = (int16_t)(Stn.current() * 100.0f);
  if (c100 != _c100) {
    Lcd.fillRect(0, 22, LCD_W, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "A %.2f", Stn.current());
    Lcd.drawText(2, 22, buf, COL_YELLOW, Pal.bg, 1);
    _c100 = c100;
  }
  int16_t p10 = (int16_t)(Stn.power() * 10.0f);
  if (p10 != _p10) {
    Lcd.fillRect(0, 32, LCD_W, 9, Pal.bg);
    snprintf(buf, sizeof(buf), "P %.1f", Stn.power());
    Lcd.drawText(2, 32, buf, COL_MAGENTA, Pal.bg, 1);
    _p10 = p10;
  }
  int16_t t1 = Stn.tipPresent() ? (int16_t)(Stn.tipTemp() + 0.5f) : -1;
  if (t1 != _t1 || Stn.tipPresent() != _tValid) {
    Lcd.fillRect(0, 42, LCD_W, 9, Pal.bg);
    if (Stn.tipPresent())
      snprintf(buf, sizeof(buf), "T %d", (int)t1);
    else
      snprintf(buf, sizeof(buf), "T ---");
    Lcd.drawText(2, 42, buf, Pal.danger, Pal.bg, 1);
    _t1 = t1;
    _tValid = Stn.tipPresent();
  }

  // 编辑值 + 箭头
  char val[16];
  if (_item->suffix)
    snprintf(val, sizeof(val), "%.1f%s", _ef, _item->suffix);
  else
    snprintf(val, sizeof(val), "%.1f", _ef);
  if (strcmp(val, _valCache) != 0) {
    Lcd.fillRect(0, 52, LCD_W, 16, Pal.bg);
    Lcd.drawText(1, 55, "<", Pal.warn, Pal.bg, 2);
    int16_t w = Lcd.textWidth(val, 2);
    Lcd.drawText((LCD_W - w) / 2, 54, val, Pal.accent, Pal.bg, 2);
    Lcd.drawText(LCD_W - 7, 55, ">", Pal.warn, Pal.bg, 2);
    strncpy(_valCache, val, sizeof(_valCache) - 1);
    _valCache[sizeof(_valCache) - 1] = 0;
  }
#endif

  // ---- 曲线(250ms 刷新, 全区重绘) ----
  uint32_t now = millis();
  if (now - _lastHistMs >= 250) {
    _lastHistMs = now;
    pushHistory();

    const int16_t gx = C_CUR_X0, gw = C_CUR_X1 - C_CUR_X0 + 1;
    const int16_t gy = C_CUR_Y0, gh = C_CUR_Y1 - C_CUR_Y0 + 1;
    Lcd.fillRect(gx, gy, gw, gh, Pal.bg);

#if LCD_W >= 200
    const int16_t yb = gy + gh - 1;
    // 双轴刻度 + 网格
    char lb[6];
    for (uint8_t i = 0; i < 5; i++) {
      int16_t y = yb - (int16_t)(gh - 1) * i / 4;
      if (i > 0 && i < 4)
        Lcd.drawHLine(gx, y, gw, Pal.grid);
      snprintf(lb, sizeof(lb), "%d", (int)(C_TMAX * i / 4));
      int16_t tw = Lcd.textWidth(lb, 1);
      Lcd.drawText(gx - 2 - tw, y - 3, lb, Pal.fg, Pal.bg, 1);
      snprintf(lb, sizeof(lb), "%d", (int)(25 * i));
      Lcd.drawText(C_CUR_X1 + 3, y - 3, lb, Pal.danger, Pal.bg, 1);
    }
#else
    Lcd.drawRect(gx, gy, gw, gh, Pal.grid);
    Lcd.drawHLine(gx + 1, gy + gh / 2, gw - 2, Pal.grid);
    Lcd.drawText(gx + 1, gy + 1, "P", COL_GREEN, Pal.bg, 1);
    Lcd.drawText(gx + 8, gy + 1, "T", COL_BLUE, Pal.bg, 1);
#endif

    int16_t plotY0 =
#if LCD_W >= 200
        gy;
#else
        gy + 9;
#endif
    int16_t plotH =
#if LCD_W >= 200
        gh;
#else
        gh - 10;
#endif
    int16_t plotYb = plotY0 + plotH - 1;

    int16_t pp = gx, py = plotYb, tp = gx, ty = plotYb;
    for (uint8_t k = 0; k < HN; k++) {
      uint8_t idx = (_head + k) % HN;
      int16_t x = gx + (int16_t)k * (gw - 1) / (HN - 1);
      int16_t pyy = plotYb - (int16_t)_powHist[idx] * (plotH - 1) / 100;
      int16_t tv = _tempHist[idx];
      if (tv > C_TMAX)
        tv = C_TMAX;
      int16_t tyy = plotYb - (int32_t)tv * (plotH - 1) / C_TMAX;
      if (k > 0) {
        Lcd.drawLine(pp, py, x, pyy, COL_GREEN);
        Lcd.drawLine(tp, ty, x, tyy, COL_BLUE);
      }
      pp = x;
      py = pyy;
      tp = x;
      ty = tyy;
    }
  }
}
