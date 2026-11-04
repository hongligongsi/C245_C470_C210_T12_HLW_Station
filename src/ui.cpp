/**
 * @file ui.cpp
 * @brief 界面实现: 主页(标准/曲线两种风格) + 故障页 + 菜单渲染 + 深/浅主题
 *        控件仅在数值变化时重绘, 配合脏区域把软 SPI 流量降到最低
 */
#include "ui.h"
#include "about.h"
#include "board.h"
#include "calib.h"
#include "font5x7.h"
#include "menu.h"
#include "settings.h"
#include "station.h"
#include "tft.h"
#include <stdio.h>
#include <string.h>

Ui Screen;
Palette Pal;

// 布局参数 (根据屏幕宽度自适应)
#define BIG_SCALE ((LCD_W >= 120) ? 4 : 3)
#define BIG_PITCH (6 * BIG_SCALE)
#define BIG_H (7 * BIG_SCALE)
#define BIG_Y (14 + ((LCD_W >= 120) ? 10 : 0))
#define SUF_SCALE ((LCD_W >= 120) ? 2 : 1)
#define SET_Y (BIG_Y + BIG_H + ((LCD_W >= 120) ? 18 : 8))
#define BAR_Y (SET_Y + 14 + ((LCD_W >= 120) ? 14 : 8))
#define BAR_H ((LCD_W >= 120) ? 8 : 6)
#define TEL_Y1 (BAR_Y + BAR_H + ((LCD_W >= 120) ? 16 : 8))
#define TEL_Y2 (TEL_Y1 + 10)

// 曲线绘图区
#define CUR_GX 2
#define CUR_GW (LCD_W - 4)
#define CUR_GY BAR_Y
#define CUR_GH (LCD_H - 11 - CUR_GY - 26)
#define CUR_TEL1 (LCD_H - 30)
#define CUR_TEL2 (LCD_H - 20)

// ---------------------------------------------------------------------------
// 320x240 横屏布局 (2.0/2.4/2.8/3.2" ST7789V2)
// ---------------------------------------------------------------------------
#if LCD_W >= 200
#define W_HDR_H 18
// 左侧竖向渐变功率条
#define W_BAR_X 18
#define W_BAR_Y 46
#define W_BAR_W 28
#define W_BAR_H 142
#define W_PCT_Y 190
// 居中大字温度(3 位槽位: 3*6*10=180px)
#define W_BIG_X 70
#define W_BIG_Y 56
#define W_BIG_SCALE 10
#define W_DEG_X 254
#define W_DEG_Y 58
#define W_SET_Y 132
// 底部三个圆角快捷温度框
#define W_CH_X0 65
#define W_CH_Y 192
#define W_CH_VAL_Y 200
#define W_CH_BOX_W 58
#define W_CH_BOX_H 30
#define W_CH_GAP 8
// 信息版布局(STYLE_CURVE): 电压/电流/功率 + 快捷温度 + 曲线
#define W2_VIP_LBL_Y 20
#define W2_VIP_VAL_Y 31
#define W2_VIP_X0 8
#define W2_VIP_X1 118
#define W2_VIP_X2 230
#define W2_CH_Y 50
#define W2_CH_VAL_Y 61
#define W2_CH_X0 6
#define W2_CH_DX 60
#define W2_BIG_X 192
#define W2_BIG_Y 50
#define W2_BIG_SCALE 7
#define W2_SET_Y 104
#define W2_CUR_X0 26
#define W2_CUR_X1 294
#define W2_CUR_Y0 120
#define W2_CUR_Y1 220
#define W2_CUR_TMAX 480 // 左轴温度量程(固定)

// 圆角矩形描边(半径 r, 只画边框不填充)
static void drawRoundBox(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, uint16_t c) {
  Lcd.drawHLine(x + r, y, w - 2 * r, c);
  Lcd.drawHLine(x + r, y + h - 1, w - 2 * r, c);
  Lcd.drawVLine(x, y + r, h - 2 * r, c);
  Lcd.drawVLine(x + w - 1, y + r, h - 2 * r, c);
  for (int16_t i = 0; i < r; i++) {
    int16_t d = r - i;
    Lcd.setPixel(x + i, y + d, c);
    Lcd.setPixel(x + w - 1 - i, y + d, c);
    Lcd.setPixel(x + i, y + h - 1 - d, c);
    Lcd.setPixel(x + w - 1 - i, y + h - 1 - d, c);
  }
}
#endif

// 大温度固定 3 位槽位
#define BIG_SLOT_W (3 * BIG_PITCH)
#define BIG_SUF_W (2 * 6 * SUF_SCALE)
#define BIG_TOTAL_W (BIG_SLOT_W + 2 + BIG_SUF_W)
#define BIG_X ((LCD_W - BIG_TOTAL_W) / 2)
#define SET_SLOT_W (3 * 12)

static void applyTheme() {
  if (Cfg.theme == THEME_LIGHT) {
    Pal.bg = COL_WHITE;
    Pal.panel = COL_WHITE; // 顶栏与背景同色(对齐参考机)
    Pal.fg = COL_BLACK;
    Pal.accent = COL_BLUE;
    Pal.ok = COL_DARKGREEN;
    Pal.warn = COL_ORANGE;
    Pal.grid = RGB565(0x90, 0x94, 0x98);
    Pal.gridMin = RGB565(0xDC, 0xDE, 0xE2);
    Pal.danger = COL_RED;
    Pal.info = COL_NAVY;
    Pal.vCol = COL_BLUE;                 // 深蓝, 白底可读
    Pal.cCol = RGB565(0xA8, 0x68, 0x00); // 暗金
    Pal.pCol = RGB565(0x88, 0x00, 0x88); // 紫
    Pal.pill = RGB565(0xCF, 0xE8, 0xD2); // 浅绿胶囊
  } else {
    Pal.bg = COL_BLACK;
    Pal.panel = COL_BLACK; // 顶栏与背景同色(对齐参考机)
    Pal.fg = COL_WHITE;
    Pal.accent = COL_YELLOW;
    Pal.ok = COL_GREEN;
    Pal.warn = COL_ORANGE;
    Pal.grid = COL_DARKGREEN;
    Pal.gridMin = RGB565(0x1C, 0x24, 0x20);
    Pal.danger = COL_RED;
    Pal.info = COL_CYAN;
    Pal.vCol = COL_BLUE;
    Pal.cCol = COL_YELLOW;
    Pal.pCol = COL_MAGENTA;
    Pal.pill = COL_DARKGREEN;
  }
}

// ---------------------------------------------------------------------------
// 8x8 单色状态图标 (顶栏, 状态变化才重绘)
// ---------------------------------------------------------------------------
static const uint8_t ICON_HOURGLASS[8] = {0xFF, 0x7E, 0x3C, 0x18,
                                          0x18, 0x3C, 0x7E, 0xFF};
static const uint8_t ICON_MOON[8] = {0x3C, 0x78, 0x70, 0x60,
                                     0x70, 0x78, 0x3C, 0x00};
static const uint8_t ICON_SPEAKER[8] = {0x18, 0x3C, 0x7E, 0xE0,
                                        0xE0, 0x7E, 0x3C, 0x18};

static void drawIcon(int16_t x, int16_t y, const uint8_t *bmp, uint16_t col) {
  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++)
      if (bmp[r] & (0x80 >> c))
        Lcd.setPixel(x + c, y + r, col);
}

static void drawFixed3(int16_t x, int16_t y, int16_t value, uint8_t scale,
                       uint16_t color, uint16_t bg, bool dash = false) {
  char buf[8];
  if (dash)
    memcpy(buf, "---", 4);
  else
    snprintf(buf, sizeof(buf), "%3d", (int)value);
  Lcd.fillRect(x, y, 3 * 6 * scale, 7 * scale, bg);
  Lcd.drawText(x, y, buf, color, bg, scale);
}

void Ui::invalidate() {
  _lastPage = 0xFF;
  _lastStyle = 0xFF;
  _lastTheme = 0xFF;
  _lastHdrIcons = 0xFF;
  _lastTipType = 0xFF;
  _lastStatus = 0xFF;
  _histDirty = true;
}

void Ui::showQuickTempPrompt(int16_t temp) {
  _quickPromptMs = millis();
  _quickPromptTemp = temp;
  _lastFooter = 0xFF;
}

void Ui::begin() {
  applyTheme();
  _lastTheme = Cfg.theme;
  _lastStyle = Cfg.style;

  // 开机画面: 双线框 + 标题
  Lcd.clear(Pal.bg);
  uint8_t ts = (LCD_W >= 200) ? 3 : 2; // 标题字号
  Lcd.drawRect(6, 6, LCD_W - 12, LCD_H - 12, Pal.info);
  Lcd.drawRect(9, 9, LCD_W - 18, LCD_H - 18, Pal.grid);
  const char *t1 = "T12/HLW";
  const char *t2 = "STATION";
  int16_t cy = LCD_H / 2;
  Lcd.drawText((LCD_W - Lcd.textWidth(t1, ts)) / 2, cy - 7 * ts - 8, t1,
               Pal.info, Pal.bg, ts);
  Lcd.drawText((LCD_W - Lcd.textWidth(t2, ts)) / 2, cy + 4, t2, Pal.fg, Pal.bg,
               ts);
  const char *t3 = "V1.7  HEAT UP";
  Lcd.drawText((LCD_W - Lcd.textWidth(t3, 1)) / 2, cy + 7 * ts + 12, t3,
               Pal.grid, Pal.bg, 1);
  Lcd.flushAll();
  delay(1200);

  Lcd.clear(Pal.bg);
  _lastPage = 0xFF;
  _lastMode = -1;
  _histDirty = true;
  _bootMs = millis(); // 开机提示"重启恢复"从界面就绪后开始计时
}

// 绘制主页静态框架
void Ui::drawStaticFrame(bool curve) {
  Lcd.fillRect(0, 0, LCD_W, 10, Pal.panel);
  drawSmallTip();
  Lcd.drawHLine(0, 10, LCD_W, Pal.grid);
  Lcd.drawText(2, SET_Y, "SET", Pal.accent, Pal.bg, 1);
  if (!curve)
    Lcd.drawRect(2, BAR_Y, LCD_W - 4, BAR_H, Pal.grid);
#if LCD_W >= 120
  Lcd.drawText(LCD_W - 42, TEL_Y2, "AMB", Pal.grid, Pal.bg, 1);
#endif
}

// 小屏顶栏左侧: 动态手柄型号 (245<->470 状态切换时跟随刷新)
void Ui::drawSmallTip() {
  uint8_t key = (uint8_t)Stn.tipType() | (Stn.hvMode() ? 0x80 : 0x00);
  if (key == _lastTipType)
    return;
  Lcd.fillRect(2, 1, 30, 9, Pal.panel);
  Lcd.drawText(2, 1, Stn.tipName(), Pal.fg, Pal.panel, 1);
  _lastTipType = key;
}

void Ui::drawStatus() {
  // 右上角模式/状态, 固定宽度槽位
  const int16_t slotW = 42;
  int16_t x = LCD_W - slotW - 1;
  Lcd.fillRect(x, 0, slotW, 9, Pal.panel);

  const char *txt;
  uint16_t col;
  if (!Stn.tipPresent()) {
    txt = "NO TIP";
    col = Pal.danger;
  } else {
    switch (Stn.mode()) {
    case MODE_HEAT:
      if (Stn.duty() > 0) {
        txt = "HEAT";
        col = Pal.warn;
      } else {
        txt = "READY";
        col = Pal.ok;
      }
      break;
    case MODE_SLEEP:
      txt = "SLEEP";
      col = Pal.info;
      break;
    case MODE_STANDBY:
      txt = "STBY";
      col = Pal.info;
      break;
    case MODE_BOOST:
      txt = "BOOST";
      col = Pal.warn;
      break;
    case MODE_FAULT:
      txt = "FAULT";
      col = Pal.danger;
      break;
    default:
      txt = "OFF";
      col = Pal.grid;
      break;
    }
  }
  int16_t w = Lcd.textWidth(txt, 1);
  Lcd.drawText(x + slotW - w, 1, txt, col, Pal.panel, 1);
}

void Ui::drawBigTemp() {
  bool valid = Stn.tipPresent();
  int16_t v = (int16_t)(Stn.tipTemp() + 0.5f);
  uint8_t colorKey;
  uint16_t col;

  if (!valid) {
    col = Pal.danger;
    colorKey = 0;
  } else if (Stn.mode() == MODE_FAULT) {
    col = Pal.danger;
    colorKey = 1;
  } else if (Stn.mode() == MODE_SLEEP || Stn.mode() == MODE_STANDBY) {
    col = Pal.accent;
    colorKey = 2;
  } else if (Stn.duty() > 0) {
    col = Pal.warn;
    colorKey = 3;
  } else {
    col = Pal.ok;
    colorKey = 4;
  }

  if (v != _lastBigTemp || valid != _lastBigValid ||
      colorKey != _lastBigColor) {
    drawFixed3(BIG_X, BIG_Y, v, BIG_SCALE, col, Pal.bg, !valid);

    // 温度后缀 °C (上标位置)
    int16_t sx = BIG_X + BIG_SLOT_W + 2;
    int16_t sy = BIG_Y + (BIG_H - 7 * SUF_SCALE) - 2;
    if (sy < BIG_Y)
      sy = BIG_Y;
    char suf[3] = {FONT_DEG, 'C', 0};
    Lcd.fillRect(sx, BIG_Y, BIG_SUF_W, BIG_H, Pal.bg);
    Lcd.drawText(sx, sy, suf, col, Pal.bg, SUF_SCALE);

    _lastBigTemp = v;
    _lastBigValid = valid;
    _lastBigColor = colorKey;
  }
}

void Ui::drawSetTemp() {
  int16_t v = Stn.setTemp();
  if (v != _lastSetTemp) {
    int16_t x = LCD_W - 2 - SET_SLOT_W - 2 - 18;
    Lcd.fillRect(x, SET_Y - 1, SET_SLOT_W + 18, 12, Pal.bg);
    char num[8];
    snprintf(num, sizeof(num), "%3d", v);
    Lcd.drawText(x, SET_Y, num, Pal.accent, Pal.bg, 2);
    char suf[3] = {FONT_DEG, 'C', 0};
    Lcd.drawText(x + SET_SLOT_W + 2, SET_Y + 4, suf, Pal.accent, Pal.bg, 1);
    _lastSetTemp = v;
  }
}

void Ui::drawDutyBar() {
  uint8_t d = Stn.duty();
  if (d != _lastDuty) {
    int16_t innerW = LCD_W - 8;
    int16_t fillW = (int16_t)((uint32_t)innerW * d / PWM_MAX);
    Lcd.fillRect(4, BAR_Y + 2, innerW, BAR_H - 4, Pal.bg);
    if (fillW > 0) {
      uint16_t c = d > 200 ? Pal.danger : (d > 100 ? Pal.warn : Pal.ok);
      Lcd.fillRect(4, BAR_Y + 2, fillW, BAR_H - 4, c);
    }
    _lastDuty = d;
  }
}

void Ui::drawTelemetry(int16_t y1, int16_t y2) {
  char buf[12];

  int16_t v10 = (int16_t)(Stn.vbus() * 10.0f);
  if (v10 != _lastVBus10) {
    Lcd.fillRect(2, y1, 40, 8, Pal.bg);
    snprintf(buf, sizeof(buf), "%2d.%dV", v10 / 10, abs(v10 % 10));
    uint16_t c =
        (Stn.vbus() < VBUS_UV_FAULT + 2.0f || Stn.vbus() > VBUS_OV_FAULT - 2.0f)
            ? Pal.danger
            : Pal.vCol;
    Lcd.drawText(2, y1, buf, c, Pal.bg, 1);
    _lastVBus10 = v10;
  }

  int16_t c10 = (int16_t)(Stn.current() * 10.0f);
  if (c10 != _lastCurr10) {
    Lcd.fillRect(LCD_W - 30, y1, 28, 8, Pal.bg);
    snprintf(buf, sizeof(buf), "%d.%dA", c10 / 10, abs(c10 % 10));
    Lcd.drawText(LCD_W - 2 - Lcd.textWidth(buf, 1), y1, buf, Pal.info, Pal.bg,
                 1);
    _lastCurr10 = c10;
  }

#if LCD_W >= 120
  int8_t amb = (int8_t)(Stn.ambient() + 0.5f);
  if (amb != _lastAmbient) {
    Lcd.fillRect(LCD_W - 24, y2, 22, 8, Pal.bg);
    snprintf(buf, sizeof(buf), "%dC", amb);
    Lcd.drawText(LCD_W - 2 - Lcd.textWidth(buf, 1), y2, buf, Pal.grid, Pal.bg,
                 1);
    _lastAmbient = amb;
  }
#endif

  // 功率百分比 (左下)
  uint8_t dp = (uint8_t)((uint32_t)Stn.duty() * 100 / PWM_MAX);
  if (dp != _lastDutyPct) {
    Lcd.fillRect(2, y2, 52, 8, Pal.bg);
    snprintf(buf, sizeof(buf), "PWR %3d%%", dp);
    Lcd.drawText(2, y2, buf, Pal.grid, Pal.bg, 1);
    _lastDutyPct = dp;
  }
}

void Ui::drawFooter() {
  uint32_t now = millis();

  // 快捷温度切换提示(1s)
  if ((int32_t)(now - _quickPromptMs) < 1000 &&
      (int32_t)(now - _quickPromptMs) >= 0) {
    char p[24];
    snprintf(p, sizeof(p), "设定 %d%cC", _quickPromptTemp, (char)FONT_DEG);
    const uint8_t id = 200;
    if (id != _lastFooter) {
      Lcd.fillRect(0, LCD_H - 13, LCD_W, 13, Pal.bg);
      Lcd.drawHLine(0, LCD_H - 14, LCD_W, Pal.grid);
      Lcd.drawText((LCD_W - Lcd.textWidth(p, 1)) / 2, LCD_H - 10, p,
                   Pal.accent, Pal.bg, 1);
      _lastFooter = id;
    }
    return;
  }

  // 常规操作提示
  uint8_t id;
  const char *txt;
  if (!Stn.tipPresent()) {
    id = 0;
    txt = "请插入工具";
  } else
    switch (Stn.mode()) {
    case MODE_HEAT:
    case MODE_BOOST:
      // 每 2.5s 交替
      if ((millis() / 2500) & 1) {
        id = 1;
        txt = "轻按切档";
      } else {
        id = 2;
        txt = "长按菜单";
      }
      break;
    case MODE_SLEEP:
      id = 3;
      txt = "长按唤醒";
      break;
    case MODE_STANDBY:
      id = 4;
      txt = "待机中";
      break;
    case MODE_FAULT:
      id = 5;
      txt = "轻按复位";
      break;
    default:
      id = 6;
      txt = "轻按加热";
      break;
    }

  if (id != _lastFooter) {
    Lcd.fillRect(0, LCD_H - 13, LCD_W, 13, Pal.bg);
    Lcd.drawHLine(0, LCD_H - 14, LCD_W, Pal.grid);
    Lcd.drawText((LCD_W - Lcd.textWidth(txt, 1)) / 2, LCD_H - 10, txt, Pal.grid,
                 Pal.bg, 1);
    _lastFooter = id;
  }
}

void Ui::drawFault() {
  const char *reason;
  char buf[24];
  switch (Stn.fault()) {
  case FAULT_VUV:
    snprintf(buf, sizeof(buf), "电压过低 %.1fV", Stn.vbus());
    reason = buf;
    break;
  case FAULT_VOV:
    snprintf(buf, sizeof(buf), "电压过高 %.1fV", Stn.vbus());
    reason = buf;
    break;
  case FAULT_OVERTEMP:
    reason = "烙铁头过温";
    break;
  case FAULT_OVERCUR:
    reason = "短路过载";
    break;
  case FAULT_TIPSENSOR:
    reason = "传感器异常";
    break;
  case FAULT_VTIP:
    reason = "烙铁头异常";
    break;
  default:
    reason = "未知故障";
    break;
  }

#if LCD_W >= 200
  // 对齐原厂故障页: 无"!"与色块, 居中两行白字(故障原因 + 重启恢复)
  const uint8_t cs = 3, as = 4;   // 汉字 3 倍(36px), ASCII 4 倍(28px)
  const uint8_t lineH = 12 * cs; // 汉字原始 12px
  const uint8_t gap = 16;
  int16_t y1 = (LCD_H - (lineH * 2 + gap)) / 2;
  int16_t w1 = Lcd.textWidth(reason, as, cs);
  Lcd.drawText((LCD_W - w1) / 2, y1, reason, Pal.fg, Pal.bg, as, cs);
  int16_t w2 = Lcd.textWidth("重启恢复", as, cs);
  Lcd.drawText((LCD_W - w2) / 2, y1 + lineH + gap, "重启恢复", Pal.fg, Pal.bg,
               as, cs);
#else
  // 小屏: 12px 汉字不放大
  const int16_t fy2 = 56;
  Lcd.drawText((LCD_W - Lcd.textWidth(reason, 1)) / 2, fy2, reason, Pal.fg,
               Pal.bg, 1);
  Lcd.drawText((LCD_W - Lcd.textWidth("重启恢复", 1)) / 2, fy2 + 24, "重启恢复",
               Pal.fg, Pal.bg, 1);
#endif
}

// ---------------------------------------------------------------------------
// 全屏状态页: 无工具 / 换芯 / 休眠
// 三者互斥并覆盖主页(优先级 故障 > 无工具 > 换芯 > 休眠), 只在状态切入时整屏
// 绘制一次; 页内动态量(休眠时的当前温度)单独按值缓存重绘, 避免软 SPI 刷整屏
// ---------------------------------------------------------------------------
void Ui::drawStatePage(const char *title, uint16_t titleCol, const char *sub,
                       const char *hint) {
#if LCD_W >= 200
  const uint8_t ts = 3, ss = 2, hs = 1; // 汉字 36px / 24px / 12px
#else
  const uint8_t ts = 2, ss = 1, hs = 1; // 小屏: 24px / 12px
#endif
  const int16_t th = 12 * ts, sh = 12 * ss;
  int16_t total = th + sh + 26 + (hint ? 12 : 0);
  int16_t y = (int16_t)((LCD_H - total) / 2);
  if (y < 2)
    y = 2;

  int16_t w = Lcd.textWidth(title, ts, ts);
  Lcd.drawText((LCD_W - w) / 2, y, title, titleCol, Pal.bg, ts, ts);
  y += th + 13;

  w = Lcd.textWidth(sub, ss, ss);
  Lcd.drawText((LCD_W - w) / 2, y, sub, Pal.fg, Pal.bg, ss, ss);
  y += sh + 13;

  if (hint) {
    w = Lcd.textWidth(hint, hs, hs);
    Lcd.drawText((LCD_W - w) / 2, y, hint, Pal.grid, Pal.bg, hs, hs);
  }
}

void Ui::drawNoTipPage() {
  drawStatePage("无工具", Pal.danger, "请插入手柄", "插入后自动识别");
}

void Ui::drawTipChangePage() {
#if LCD_W >= 200
  drawStatePage("换芯中", Pal.warn, "加热已停止", "装好烙铁头后自动恢复");
#else
  drawStatePage("换芯中", Pal.warn, "加热已停止", "装好自动恢复");
#endif
}

void Ui::drawSleepPage() {
  _lastSleepTemp = 0x7FFF; // 强制下面的温度行重画
#if LCD_W >= 200
  drawStatePage("已休眠", Pal.info, "旋转或短按唤醒", nullptr);
#else
  drawStatePage("已休眠", Pal.info, "旋转唤醒", nullptr);
#endif
  drawSleepTemp();
}

// 休眠页当前温度: 只在整数温度变化时重绘一行
void Ui::drawSleepTemp() {
  int16_t v = (int16_t)(Stn.tipTemp() + 0.5f);
  if (v == _lastSleepTemp)
    return;
  _lastSleepTemp = v;

#if LCD_W >= 200
  const uint8_t sc = 2;
  const int16_t h = 7 * sc;
#else
  const uint8_t sc = 1;
  const int16_t h = 7 * sc;
#endif
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%cC", (int)v, FONT_DEG);
  int16_t w = Lcd.textWidth(buf, sc, 1);
#if LCD_W >= 200
  int16_t y = LCD_H - h - 26;
#else
  int16_t y = LCD_H - h - 12;
#endif
  // 先清一贯穿整屏的横条, 再居中写字(旧数字比新数字宽时不会留残影)
  Lcd.fillRect(0, y - 2, LCD_W, h + 4, Pal.bg);
  Lcd.drawText((LCD_W - w) / 2, y, buf, Pal.info, Pal.bg, sc, 1);
}

// ---------------------------------------------------------------------------
// 曲线风格主页: 绿线=功率%, 蓝线=温度
// ---------------------------------------------------------------------------
void Ui::pushHistory() {
  _powHist[_histHead] = (uint8_t)((uint32_t)Stn.duty() * 100 / PWM_MAX);
  int16_t t = (int16_t)(Stn.tipTemp() + 0.5f);
  if (t < 0)
    t = 0;
  if (t > 500)
    t = 500;
  _tempHist[_histHead] = t;
  _histHead = (_histHead + 1) % HIST_N;
  _histDirty = true;
}

void Ui::drawCurve() {
  uint32_t now = millis();
  if (now - _lastHistMs >= 250) {
    _lastHistMs = now;
    pushHistory();
  }
  if (!_histDirty)
    return;
  _histDirty = false;

  // 清绘图区 + 边框/中线
  Lcd.fillRect(CUR_GX, CUR_GY, CUR_GW, CUR_GH, Pal.bg);
  Lcd.drawRect(CUR_GX, CUR_GY, CUR_GW, CUR_GH, Pal.grid);
  Lcd.drawHLine(CUR_GX + 1, CUR_GY + CUR_GH / 2, CUR_GW - 2, Pal.grid);

  uint16_t powCol = Pal.ok;
  uint16_t tmpCol = Pal.vCol;

  // 图例
  Lcd.drawText(CUR_GX + 2, CUR_GY + 1, "P", powCol, Pal.bg, 1);
  Lcd.drawText(CUR_GX + 10, CUR_GY + 1, "T", tmpCol, Pal.bg, 1);

  int16_t plotX0 = CUR_GX + 1;
  int16_t plotY0 = CUR_GY + 9; // 给图例留一行
  int16_t plotW = CUR_GW - 2;
  int16_t plotH = CUR_GH - 10;
  int16_t plotYb = plotY0 + plotH - 1;

  // 纵向网格 3 条(次级色)
  for (uint8_t i = 1; i < 4; i++) {
    int16_t x = plotX0 + (int16_t)(plotW - 1) * i / 4;
    Lcd.drawVLine(x, plotY0, plotH, Pal.gridMin);
  }

  // 设定温度虚线参考线
  if (Stn.tipPresent() &&
      (Stn.mode() == MODE_HEAT || Stn.mode() == MODE_BOOST ||
       Stn.mode() == MODE_STANDBY)) {
    int16_t tmax = (Cfg.tempMax > 0 ? Cfg.tempMax : TEMP_MAX_C);
    int16_t set = Stn.setTemp();
    if (set > 0 && set <= tmax) {
      int16_t sy = plotYb - (int32_t)set * (plotH - 1) / tmax;
      for (int16_t x = plotX0; x < plotX0 + plotW - 4; x += 6)
        Lcd.drawHLine(x, sy, 3, Pal.accent);
    }
  }

  int16_t prevPx = plotX0, prevPy = plotYb;
  int16_t prevTx = plotX0, prevTy = plotYb;
  for (uint8_t k = 0; k < HIST_N; k++) {
    uint8_t idx = (_histHead + k) % HIST_N;
    int16_t x = plotX0 + (int16_t)k * (plotW - 1) / (HIST_N - 1);

    int16_t py = plotYb - (int16_t)_powHist[idx] * (plotH - 1) / 100;
    int16_t ty = plotYb - (int32_t)_tempHist[idx] * (plotH - 1) /
                              (Cfg.tempMax > 0 ? Cfg.tempMax : TEMP_MAX_C);

    if (k > 0) {
      Lcd.drawLine(prevPx, prevPy, x, py, powCol);
      Lcd.drawLine(prevTx, prevTy, x, ty, tmpCol);
    }
    prevPx = x;
    prevPy = py;
    prevTx = x;
    prevTy = ty;
  }
}

void Ui::drawStandard() {
  drawDutyBar();
  drawTelemetry(TEL_Y1, TEL_Y2);
}

#if LCD_W >= 200
// ---------------------------------------------------------------------------
// 320x240 宽屏主页 (对齐参考机布局)
// ---------------------------------------------------------------------------
// 宽屏静态框架: info=true 信息版(STYLE_CURVE), false 简洁版(STYLE_BAR)
void Ui::drawWideFrame(bool info) {
  // 顶栏(与背景同色): 型号与状态图标由 drawWideTip / drawWideIcons 绘制
  Lcd.fillRect(0, 0, LCD_W, W_HDR_H, Pal.panel);
  _lastStatus = 0xFF; // 顶栏被整体擦除, 状态文字需重画

  if (info) {
    // V/I/P 标签
    Lcd.drawText(W2_VIP_X0, W2_VIP_LBL_Y, "电压", Pal.grid, Pal.bg, 1);
    Lcd.drawText(W2_VIP_X1, W2_VIP_LBL_Y, "电流", Pal.grid, Pal.bg, 1);
    Lcd.drawText(W2_VIP_X2, W2_VIP_LBL_Y, "功率", Pal.grid, Pal.bg, 1);
    Lcd.drawVLine(W2_VIP_X1 - 8, W2_VIP_LBL_Y - 2, 28, Pal.gridMin);
    Lcd.drawVLine(W2_VIP_X2 - 8, W2_VIP_LBL_Y - 2, 28, Pal.gridMin);
#ifndef NO_INPUT
    // 快捷温度通道标签
    for (uint8_t i = 0; i < 3; i++) {
      char lb[12];
      snprintf(lb, sizeof(lb), "快捷%u", (unsigned)(i + 1));
      Lcd.drawText(W2_CH_X0 + (int16_t)i * W2_CH_DX, W2_CH_Y, lb, Pal.grid,
                   Pal.bg, 1);
    }
#endif
    Lcd.drawHLine(0, W2_CUR_Y1 + 3, LCD_W, Pal.grid);
  } else {
    // 左侧功率条外框(渐变填充在 drawWideBar 中完成)
    Lcd.drawRect(W_BAR_X - 1, W_BAR_Y - 1, W_BAR_W + 2, W_BAR_H + 2, Pal.grid);
  }
}

void Ui::drawWideTip() {
  // 键值含 hvMode: 245<->470 状态切换时顶栏型号跟随刷新
  uint8_t key = (uint8_t)Stn.tipType() | (Stn.hvMode() ? 0x80 : 0x00);
  if (key == _lastTipType)
    return;
  Lcd.fillRect(2, 1, 90, W_HDR_H - 2, Pal.panel);
  Lcd.drawText(4, 2, Stn.tipName(), Pal.fg, Pal.panel, 2);
  if (_lastTipType != 0xFF) {
    _tipChgMs = millis(); // 型号切换: 顶栏短暂显示"换芯中"
    _lastStatus = 0xFF;
  }
  _lastTipType = key;
}

// 顶栏状态提示文字(右对齐到图标左侧 x=244)
void Ui::drawWideStatus() {
  uint32_t now = millis();
  uint8_t id;
  const char *txt;
  uint16_t col;

  if ((int32_t)(now - _bootMs) < 1500) {
    id = 1;
    txt = "重启恢复";
    col = Pal.info;
  } else if ((int32_t)(now - _tipChgMs) < 1200) {
    id = 2;
    txt = "换芯中";
    col = Pal.warn;
  } else if (!Stn.tipPresent()) {
    id = 3;
    txt = "无工具"; // 顶栏状态; 底部提示仍为"请插入工具"
    col = Pal.danger;
  } else if (Stn.fault() == FAULT_OVERCUR) {
    id = 4;
    txt = "短路过载";
    col = Pal.danger;
  } else if (Stn.mode() == MODE_FAULT) {
    id = 5;
    txt = "故障";
    col = Pal.danger;
  } else if (Stn.mode() == MODE_SLEEP) {
    id = 6;
    txt = "已休眠";
    col = Pal.info;
  } else if (Stn.mode() == MODE_STANDBY) {
    id = 7;
    txt = "待机中";
    col = Pal.info;
  } else if (Stn.duty() > 0) {
    id = 8;
    txt = "加热中";
    col = Pal.warn;
  } else {
    id = 9;
    txt = "停止加热";
    col = Pal.grid;
  }

  if (id == _lastStatus)
    return;
  // 清除状态区(左边界避开型号文字, 右边界避开图标)
  Lcd.fillRect(60, 1, 188, W_HDR_H - 2, Pal.panel);
  int16_t w = Lcd.textWidth(txt, 1);
  Lcd.drawText(244 - w, 3, txt, col, Pal.panel, 1);
  _lastStatus = id;
}

// 顶栏右侧: 沙漏(待机计时) / 月亮(休眠) / 喇叭(声音)
void Ui::drawWideIcons() {
  // bit0=声音  bit1=休眠闩锁  bit2=待机已启用(灰) bit3=待机进行中(亮)
  uint8_t key = Cfg.soundOn ? 0x01 : 0x00;
  if (Stn.holderSleep() || Stn.mode() == MODE_SLEEP)
    key |= 0x02;
  if (Cfg.standbyTimeMin > 0 && Stn.tipPresent() && Stn.mode() != MODE_FAULT) {
    key |= (Stn.mode() == MODE_STANDBY) ? 0x0C : 0x04;
  }
  if (key == _lastHdrIcons)
    return;
  _lastHdrIcons = key;

  const int16_t y = (W_HDR_H - 8) / 2;
  Lcd.fillRect(248, 0, 72, W_HDR_H, Pal.panel);

  if (key & 0x0C)
    drawIcon(252, y, ICON_HOURGLASS, (key & 0x08) ? Pal.info : Pal.grid);
  if (key & 0x02)
    drawIcon(268, y, ICON_MOON, Pal.info);

  // 喇叭: 开启带声波, 关闭打 X
  drawIcon(290, y, ICON_SPEAKER, (key & 0x01) ? Pal.fg : Pal.grid);
  if (key & 0x01)
    Lcd.drawText(299, y, "))", Pal.fg, Pal.panel, 1);
  else
    Lcd.drawText(299, y, "X", Pal.danger, Pal.panel, 1);
}

// ---- 信息版(STYLE_CURVE) ----
void Ui::drawWideInfoVIP() {
  char buf[12];

  int16_t v10 = (int16_t)(Stn.vbus() * 10.0f);
  if (v10 != _lastVBus10) {
    Lcd.fillRect(W2_VIP_X0, W2_VIP_VAL_Y, 100, 15, Pal.bg);
    snprintf(buf, sizeof(buf), "%.1fV", Stn.vbus());
    uint16_t c =
        (Stn.vbus() < VBUS_UV_FAULT + 2.0f || Stn.vbus() > VBUS_OV_FAULT - 2.0f)
            ? Pal.danger
            : Pal.vCol;
    Lcd.drawText(W2_VIP_X0, W2_VIP_VAL_Y, buf, c, Pal.bg, 2);
    _lastVBus10 = v10;
  }

  int16_t c100 = (int16_t)(Stn.current() * 100.0f);
  if (c100 != _lastCurr100) {
    Lcd.fillRect(W2_VIP_X1, W2_VIP_VAL_Y, 104, 15, Pal.bg);
    snprintf(buf, sizeof(buf), "%.2fA", Stn.current());
    Lcd.drawText(W2_VIP_X1, W2_VIP_VAL_Y, buf, Pal.cCol, Pal.bg, 2);
    _lastCurr100 = c100;
  }

  int16_t p10 = (int16_t)(Stn.power() * 10.0f);
  if (p10 != _lastPwr10) {
    Lcd.fillRect(W2_VIP_X2, W2_VIP_VAL_Y, 88, 15, Pal.bg);
    snprintf(buf, sizeof(buf), "%.1fW", Stn.power());
    Lcd.drawText(W2_VIP_X2, W2_VIP_VAL_Y, buf, Pal.pCol, Pal.bg, 2);
    _lastPwr10 = p10;
  }
}

void Ui::drawWideInfoCh() {
  bool chg = (_lastQuickIdx != Stn.quickIdx());
  for (uint8_t i = 0; i < 3; i++)
    chg = chg || (_lastCh[i] != Cfg.quickTemp[i]);
  if (!chg)
    return;

  uint8_t idx = Stn.quickIdx();
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = W2_CH_X0 + (int16_t)i * W2_CH_DX;
    bool act = (i == idx);
    Lcd.fillRect(x - 2, W2_CH_Y - 2, W2_CH_DX - 2, 28, Pal.bg);
    // 当前通道仅文字高亮, 无底色/边框
    uint16_t col = act ? Pal.ok : Pal.grid;
    char lb[12];
    snprintf(lb, sizeof(lb), "快捷%d", i + 1);
    Lcd.drawText(x + 2, W2_CH_Y, lb, col, Pal.bg, 1);
    char num[8];
    snprintf(num, sizeof(num), "%d", (int)Cfg.quickTemp[i]);
    Lcd.drawText(x + 2, W2_CH_VAL_Y, num, col, Pal.bg, 2);
    _lastCh[i] = Cfg.quickTemp[i];
  }
  _lastQuickIdx = idx;
}

void Ui::drawWideInfoBig() {
  bool valid = Stn.tipPresent();
  int16_t v = (int16_t)(Stn.tipTemp() + 0.5f);
  uint8_t colorKey;
  uint16_t col;
  if (!valid) {
    col = Pal.danger;
    colorKey = 0;
  } else if (Stn.mode() == MODE_FAULT) {
    col = Pal.danger;
    colorKey = 1;
  } else if (Stn.mode() == MODE_SLEEP || Stn.mode() == MODE_STANDBY) {
    col = Pal.accent;
    colorKey = 2;
  } else if (Stn.duty() > 0) {
    col = Pal.danger; // 加热中: 红色大字
    colorKey = 3;
  } else {
    col = Pal.ok;
    colorKey = 4;
  }

  if (v == _lastBigTemp && valid == _lastBigValid && colorKey == _lastBigColor)
    return;

  char buf[8];
  if (!valid)
    memcpy(buf, "---", 4);
  else
    snprintf(buf, sizeof(buf), "%3d", v);
  Lcd.fillRect(W2_BIG_X - 2, W2_BIG_Y - 2, 128, 7 * W2_BIG_SCALE + 4, Pal.bg);
  Lcd.drawText(W2_BIG_X, W2_BIG_Y, buf, col, Pal.bg, W2_BIG_SCALE);

  _lastBigTemp = v;
  _lastBigValid = valid;
  _lastBigColor = colorKey;
}

void Ui::drawWideInfoSet() {
  int16_t v = Stn.setTemp();
  if (v == _lastSetTemp)
    return;
  Lcd.fillRect(W2_BIG_X - 2, W2_SET_Y - 2, 128, 18, Pal.bg);
  int16_t x = W2_BIG_X;
  Lcd.drawText(x, W2_SET_Y, "设定", Pal.accent, Pal.bg, 2);
  x += Lcd.textWidth("设定 ", 2);
  char num[8];
  snprintf(num, sizeof(num), "%d", (int)v);
  Lcd.drawText(x, W2_SET_Y, num, Pal.ok, Pal.bg, 2);
  x += Lcd.textWidth(num, 2);
  char suf[3] = {FONT_DEG, 'C', 0};
  Lcd.drawText(x + 2, W2_SET_Y + 6, suf, Pal.ok, Pal.bg, 1);
  _lastSetTemp = v;
}

// ---- 简洁版(STYLE_BAR) ----
void Ui::drawWideBoxes() {
  bool chg = (_lastQuickIdx != Stn.quickIdx());
  for (uint8_t i = 0; i < 3; i++)
    chg = chg || (_lastCh[i] != Cfg.quickTemp[i]);
  if (!chg)
    return;

  uint8_t idx = Stn.quickIdx();
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = W_CH_X0 + (int16_t)i * (W_CH_BOX_W + W_CH_GAP);
    bool act = (i == idx);
    // 圆角框: 当前档青色高亮, 其余暗色描边
    Lcd.fillRect(x, W_CH_Y, W_CH_BOX_W, W_CH_BOX_H, Pal.bg);
    drawRoundBox(x, W_CH_Y, W_CH_BOX_W, W_CH_BOX_H, 4,
                 act ? Pal.info : Pal.grid);
    char num[8];
    snprintf(num, sizeof(num), "%d", (int)Cfg.quickTemp[i]);
    int16_t tw = Lcd.textWidth(num, 2);
    Lcd.drawText(x + (W_CH_BOX_W - tw) / 2, W_CH_VAL_Y, num,
                 act ? Pal.info : Pal.fg, Pal.bg, 2);
    _lastCh[i] = Cfg.quickTemp[i];
  }
  _lastQuickIdx = idx;
}

void Ui::drawWideBig() {
  bool valid = Stn.tipPresent();
  int16_t v = (int16_t)(Stn.tipTemp() + 0.5f);
  uint8_t colorKey;
  uint16_t col;
  if (!valid) {
    col = Pal.danger;
    colorKey = 0;
  } else if (Stn.mode() == MODE_FAULT) {
    col = Pal.danger;
    colorKey = 1;
  } else if (Stn.mode() == MODE_SLEEP || Stn.mode() == MODE_STANDBY) {
    col = Pal.accent;
    colorKey = 2;
  } else {
    col = Pal.fg; // 正常状态白色大字(对齐参考机)
    colorKey = 3;
  }

  if (v == _lastBigTemp && valid == _lastBigValid && colorKey == _lastBigColor)
    return;

  char buf[8];
  if (!valid)
    memcpy(buf, "---", 4);
  else
    snprintf(buf, sizeof(buf), "%3d", v);
  // 大字与右上 °C 一并擦除重绘
  Lcd.fillRect(W_BIG_X - 4, W_BIG_Y - 6, 3 * 6 * W_BIG_SCALE + 46,
               7 * W_BIG_SCALE + 12, Pal.bg);
  Lcd.drawText(W_BIG_X, W_BIG_Y, buf, col, Pal.bg, W_BIG_SCALE);
  char suf[3] = {FONT_DEG, 'C', 0};
  Lcd.drawText(W_DEG_X, W_DEG_Y, suf, col, Pal.bg, 3);

  _lastBigTemp = v;
  _lastBigValid = valid;
  _lastBigColor = colorKey;
}

void Ui::drawWideSet() {
  int16_t v = Stn.setTemp();
  if (v == _lastSetTemp)
    return;
  char buf[20];
  snprintf(buf, sizeof(buf), "设定 %d%cC", (int)v, (char)FONT_DEG);
  int16_t w = Lcd.textWidth(buf, 2);
  // 固定区域擦除, 避免数字位数变化残留
  Lcd.fillRect(60, W_SET_Y - 2, 200, 18, Pal.bg);
  Lcd.drawText((LCD_W - w) / 2, W_SET_Y, buf, Pal.ok, Pal.bg, 2);
  _lastSetTemp = v;
}

void Ui::drawWideBar() {
  uint8_t dp = (uint8_t)((uint32_t)Stn.duty() * 100 / PWM_MAX);
  if (dp == _lastDutyPct)
    return;
  _lastDutyPct = dp;

  const int16_t x = W_BAR_X, y = W_BAR_Y, h = W_BAR_H;
  // 未点亮部分: 暗色底槽
  Lcd.fillRect(x, y, W_BAR_W, h, Pal.gridMin);
  // 自底部向上渐变填充: 顶部黄 -> 底部红
  int16_t fillH = (int16_t)((int32_t)h * dp / 100);
  for (int16_t i = 0; i < fillH; i++) {
    int16_t yy = y + h - 1 - i;
    uint8_t g = (uint8_t)(255 * i / (h - 1));
    Lcd.drawHLine(x, yy, W_BAR_W, RGB565(255, g, 0));
  }

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)dp);
  int16_t tw = Lcd.textWidth(buf, 2);
  Lcd.fillRect(4, W_PCT_Y - 2, 60, 18, Pal.bg);
  Lcd.drawText(W_BAR_X + (W_BAR_W - tw) / 2, W_PCT_Y, buf, Pal.ok, Pal.bg, 2);
}
// 信息版曲线: 双轴网格(左温度 0..480, 右功率 0..100), 绿=功率% 蓝=温度
void Ui::drawWideCurve() {
  uint32_t now = millis();
  if (now - _lastHistMs >= 250) {
    _lastHistMs = now;
    pushHistory();
  }
  if (!_histDirty)
    return;
  _histDirty = false;

  const int16_t gx = W2_CUR_X0, gw = W2_CUR_X1 - W2_CUR_X0 + 1;
  const int16_t gy = W2_CUR_Y0, gh = W2_CUR_Y1 - W2_CUR_Y0 + 1;
  const int16_t yb = gy + gh - 1;

  Lcd.fillRect(gx, gy, gw, gh, Pal.bg);

  char lb[6];
  for (uint8_t i = 0; i < 5; i++) {
    int16_t y = yb - (int16_t)(gh - 1) * i / 4;
    if (i > 0 && i < 4)
      Lcd.drawHLine(gx, y, gw, Pal.grid);
    int16_t tv = W2_CUR_TMAX * i / 4;
    snprintf(lb, sizeof(lb), "%d", (int)tv);
    int16_t tw = Lcd.textWidth(lb, 1);
    Lcd.drawText(gx - 6 - tw, y - 3, lb, Pal.fg, Pal.bg, 1);
    snprintf(lb, sizeof(lb), "%d", (int)(25 * i));
    Lcd.drawText(W2_CUR_X1 + 5, y - 3, lb, Pal.fg, Pal.bg, 1);
    Lcd.drawHLine(gx - 3, y, 3, Pal.danger);
    Lcd.drawHLine(W2_CUR_X1 + 1, y, 3, Pal.danger);
  }
  for (uint8_t i = 1; i < 8; i++) {
    int16_t x = gx + (int16_t)(gw - 1) * i / 8;
    Lcd.drawVLine(x, gy, gh, Pal.gridMin);
  }

  // 设定温度虚线参考线
  if (Stn.tipPresent() &&
      (Stn.mode() == MODE_HEAT || Stn.mode() == MODE_BOOST ||
       Stn.mode() == MODE_STANDBY)) {
    int16_t set = Stn.setTemp();
    if (set > 0 && set <= W2_CUR_TMAX) {
      int16_t sy = yb - (int32_t)set * (gh - 1) / W2_CUR_TMAX;
      for (int16_t x = gx; x < gx + gw - 5; x += 7)
        Lcd.drawHLine(x, sy, 4, Pal.accent);
    }
  }

  uint16_t powCol = Pal.ok;
  uint16_t tmpCol = Pal.vCol;

  int16_t prevPx = gx, prevPy = yb, prevTx = gx, prevTy = yb;
  for (uint8_t k = 0; k < HIST_N; k++) {
    uint8_t idx = (_histHead + k) % HIST_N;
    int16_t x = gx + (int16_t)k * (gw - 1) / (HIST_N - 1);

    int16_t py = yb - (int16_t)_powHist[idx] * (gh - 1) / 100;
    int16_t tv = _tempHist[idx];
    if (tv > W2_CUR_TMAX)
      tv = W2_CUR_TMAX;
    int16_t ty = yb - (int32_t)tv * (gh - 1) / W2_CUR_TMAX;

    if (k > 0) {
      Lcd.drawLine(prevPx, prevPy, x, py, powCol);
      Lcd.drawLine(prevTx, prevTy, x, ty, tmpCol);
    }
    prevPx = x;
    prevPy = py;
    prevTx = x;
    prevTy = ty;
  }
}
#endif // LCD_W >= 200

void Ui::update() {
  uint32_t now = millis();

  // 全屏校准页: 覆盖菜单, 不做 100ms 节流(保证旋钮跟手, 页内自带缓存)
  if (Calib.active()) {
    Calib.render();
    Lcd.present();
    return;
  }

  // 全屏「关于本机」信息页: 覆盖菜单(500ms 节流即可, 只读页变化少)
  if (About.active()) {
    static uint32_t s_aboutLast = 0;
    if (now - s_aboutLast >= 500) {
      s_aboutLast = now;
      About.render();
      Lcd.present();
    }
    return;
  }

  // 菜单激活: 不画主页(菜单渲染不做 100ms 节流, 保证跟手)
  if (MenuCtl.active()) {
    MenuCtl.render();
    Lcd.present();
    return;
  }

  if (now - _lastMs < 100)
    return;
  _lastMs = now;

  // 主题/风格切换检测
  if (Cfg.theme != _lastTheme) {
    applyTheme();
    _lastTheme = Cfg.theme;
    _lastPage = 0xFF;
  }
  if (Cfg.style != _lastStyle) {
    _lastStyle = Cfg.style;
    _lastPage = 0xFF;
  }

  // 页面分派: 0=主页 1=故障 2=无工具 3=换芯 4=休眠
  // 后三者是独立全屏状态页, 只有状态切换时才整屏重绘
  // 优先级: 故障 > 换芯 > 无工具 > 休眠
  // (换芯时用户通常会拔下烙铁头, 此时显示"换芯中"比"无工具"更有指导意义)
  uint8_t page = 0;
  if (Stn.mode() == MODE_FAULT)
    page = 1;
  else if (Stn.tipSwitchOpen())
    page = 3;
  else if (!Stn.tipPresent())
    page = 2;
  else if (Stn.mode() == MODE_SLEEP)
    page = 4;
  if (page != _lastPage) {
    Lcd.clear(Pal.bg);
    if (page == 0) {
#if LCD_W >= 200
      drawWideFrame(Cfg.style == STYLE_CURVE);
#else
      drawStaticFrame(Cfg.style == STYLE_CURVE);
#endif
      _histDirty = true;
    }
    // 强制所有控件重绘
    _lastMode = -1;
    _lastBigTemp = 0x7FFF;
    _lastBigValid = !Stn.tipPresent();
    _lastBigColor = 0xFF;
    _lastSetTemp = -1;
    _lastDuty = 0xFF;
    _lastVBus10 = -1;
    _lastCurr10 = -1;
    _lastCurr100 = -1;
    _lastPwr10 = -1;
    _lastTipType = 0xFF;
    _lastHdrIcons = 0xFF;
    _lastCh[0] = _lastCh[1] = _lastCh[2] = -1;
    _lastQuickIdx = 0xFF;
    _lastAmbient = -100;
    _lastDutyPct = 0xFF;
    _lastFooter = 0xFF;
    _lastPage = page;
  }

  if (page == 0) {
#if LCD_W >= 200
    drawWideTip();
    drawWideIcons();
    drawWideStatus();
    if (Cfg.style == STYLE_CURVE) {
      // 信息版: 电压/电流/功率 + 快捷温度 + 曲线
      drawWideInfoVIP();
      drawWideInfoBig();
      drawWideInfoSet();
#ifndef NO_INPUT
      drawWideInfoCh();
#endif
      drawWideCurve();
    } else {
      // 简洁版: 左侧渐变功率条 + 居中大字 + 圆角快捷温度
      drawWideBig();
      drawWideSet();
#ifndef NO_INPUT
      drawWideBoxes();
#endif
      drawWideBar();
    }
#else
    drawSmallTip();
    drawStatus();
    drawBigTemp();
    drawSetTemp();
    if (Cfg.style == STYLE_CURVE) {
      drawCurve();
      drawTelemetry(CUR_TEL1, CUR_TEL2);
    } else {
      drawStandard();
    }
#endif
    drawFooter();
  } else if (page == 1) {
    drawFault();
  } else if (page == 2) {
    drawNoTipPage();
  } else if (page == 3) {
    drawTipChangePage();
  } else {
    drawSleepPage();
  }

  // 休眠页: 温度是唯一动态量, 每 500ms 检查一次(值变了才重绘那一行)
  if (page == 4) {
    static uint32_t s_sleepMs = 0;
    if (now - s_sleepMs >= 500) {
      s_sleepMs = now;
      drawSleepTemp();
    }
  }

  Lcd.present();
}
