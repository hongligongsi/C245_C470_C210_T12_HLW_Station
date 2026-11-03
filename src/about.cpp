/**
 * @file about.cpp
 * @brief 全屏「关于本机」信息页实现
 *
 * 所有数据取自现有单例(Stn/Cfg), 本页不做任何采样与写入。
 * 宽屏(>=200px): 标签左对齐 + 数值右对齐; 小屏: 单列紧凑文本。
 */
#include "about.h"
#include "board.h"
#include "menu.h"
#include "settings.h"
#include "station.h"
#include "tft.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

AboutPage About;

namespace {
// 宽屏行距/标题栏
#if LCD_W >= 200
constexpr int16_t A_HDR_H = 20;
constexpr int16_t A_Y0 = 26;
constexpr int16_t A_ROW_H = 20;
#else
constexpr int16_t A_HDR_H = 10;
constexpr int16_t A_Y0 = 12;
constexpr int16_t A_ROW_H = 10;
#endif
} // namespace

void AboutPage::enter() {
  _active = true;
  _needsClear = true;
  for (auto &r : _rows)
    r[0] = 0;
}

void AboutPage::exit() {
  _active = false;
  _needsClear = true;
}

void AboutPage::select() { exit(); }

// 组装一行: "标签" + "值", 宽屏标签/值分开画, 小屏拼成一行
static void fmtRow(uint8_t i, char *label, size_t ln, char *val, size_t vn) {
  char buf[16];
  val[0] = 0;
  switch (i) {
  case 0:
    snprintf(label, ln, "固件版本");
    snprintf(val, vn, "v%s", FW_VERSION);
    break;
  case 1:
    snprintf(label, ln, "编译时间");
    snprintf(val, vn, "%s", FW_BUILD_TIME);
    break;
  case 2:
    snprintf(label, ln, "主控");
    snprintf(val, vn, "%s", MCU_NAME);
    break;
  case 3:
    snprintf(label, ln, "配置版本");
    snprintf(val, vn, "v%u", (unsigned)SETTINGS_VERSION);
    break;
  case 4:
    snprintf(label, ln, "当前手柄");
    if (Stn.tipPresent())
      snprintf(val, vn, "%s", Stn.tipName());
    else
      snprintf(val, vn, "未插入");
    break;
  case 5:
    snprintf(label, ln, "供电电压");
    snprintf(buf, sizeof(buf), "%.1f V", (double)Stn.vbus());
    if (Stn.hvMode())
      snprintf(val, vn, "%s 高压", buf);
    else
      snprintf(val, vn, "%s", buf);
    break;
  case 6:
    snprintf(label, ln, "焊头温度");
    if (Stn.tipPresent())
      snprintf(val, vn, "%d C", (int)(Stn.tipTemp() + 0.5f));
    else
      snprintf(val, vn, "---");
    break;
  case 7:
    snprintf(label, ln, "环境温度");
    snprintf(val, vn, "%d C", (int)(Stn.ambient() + 0.5f));
    break;
  default: {
    snprintf(label, ln, "运行时长");
    uint32_t s = millis() / 1000;
    uint16_t h = s / 3600;
    uint8_t m = (s % 3600) / 60, sec = s % 60;
    if (h)
      snprintf(val, vn, "%uh%02um", h, m);
    else
      snprintf(val, vn, "%02u:%02u", m, sec);
    break;
  }
  }
  (void)buf;
}

void AboutPage::render() {
  if (!_active)
    return;

  if (_needsClear) {
    Lcd.clear(Pal.bg);
    // 标题栏(反色, 与菜单一致)
    Lcd.fillRect(0, 0, LCD_W, A_HDR_H, Pal.fg);
#if LCD_W >= 200
    Lcd.drawText(3, (A_HDR_H - 14) / 2, "关于本机", Pal.bg, Pal.fg, 2);
    const char *hint = "短按返回";
    int16_t hw = Lcd.textWidth(hint, 1);
    Lcd.drawText(LCD_W - hw - 4, A_HDR_H / 2 - 3, hint, Pal.bg, Pal.fg, 1);
#else
    Lcd.drawText(2, 2, "关于本机", Pal.bg, Pal.fg, 1);
#endif
    _needsClear = false;
    for (auto &r : _rows)
      r[0] = 0; // 首帧强制重画所有行
  }

  // 标签最多 4 个汉字(12 字节), 需留结束符; 行内拼接再留 1 字节分隔符
  char label[16], val[24], line[48];
  for (uint8_t i = 0; i < ROW_N; i++) {
    int16_t y = A_Y0 + i * A_ROW_H;
#if LCD_W >= 200
    fmtRow(i, label, sizeof(label), val, sizeof(val));
    snprintf(line, sizeof(line), "%s|%s", label, val);
#else
    fmtRow(i, label, sizeof(label), val, sizeof(val));
    snprintf(line, sizeof(line), "%s:%s", label, val);
#endif
    if (strncmp(line, _rows[i], sizeof(_rows[i])) == 0)
      continue; // 本行无变化

    Lcd.fillRect(0, y, LCD_W, A_ROW_H, Pal.bg);
#if LCD_W >= 200
    uint8_t scale = 1;
    Lcd.drawText(6, y + (A_ROW_H - 9) / 2 + 1, label, Pal.fg, Pal.bg, scale);
    int16_t vw = Lcd.textWidth(val, scale);
    Lcd.drawText(LCD_W - vw - 6, y + (A_ROW_H - 9) / 2 + 1, val, Pal.info,
                 Pal.bg, scale);
#else
    Lcd.drawText(2, y, line, (i == 0) ? Pal.ok : Pal.fg, Pal.bg, 1);
#endif
    strncpy(_rows[i], line, sizeof(_rows[i]) - 1);
    _rows[i][sizeof(_rows[i]) - 1] = 0;
  }
}
