/**
 * @file tft.h
 * @brief 软件 SPI 彩屏驱动 (ST7735 80x160 / ST7789 135x240 / 240x240)
 *        带 RGB565 帧缓冲, 脏区域刷新
 */
#pragma once
#include <Arduino.h>

#ifndef LCD_TYPE
#define LCD_TYPE                                                               \
  0 // 0=ST7735 80x160  1=ST7789 135x240  2=ST7789 240x240  3=ST7789 320x240
    // 横屏
#endif

#if LCD_TYPE == 0
#define LCD_W 80
#define LCD_H 160
#define LCD_CTRL_ST7735
#elif LCD_TYPE == 1
#define LCD_W 135
#define LCD_H 240
#define LCD_CTRL_ST7789
#elif LCD_TYPE == 2
#define LCD_W 240
#define LCD_H 240
#define LCD_CTRL_ST7789
#elif LCD_TYPE == 3
#define LCD_W 320
#define LCD_H 240
#define LCD_CTRL_ST7789
#define LCD_LANDSCAPE 1 // GRAM 行列交换(MADCTL MV=1), setWindow 需交换 x/y
#else
#error "Unsupported LCD_TYPE"
#endif

// 颜色 RGB565
#define RGB565(r, g, b)                                                        \
  (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define COL_BLACK 0x0000
#define COL_WHITE 0xFFFF
#define COL_RED 0xF800
#define COL_GREEN 0x07E0
#define COL_BLUE 0x001F
#define COL_YELLOW 0xFFE0
#define COL_CYAN 0x07FF
#define COL_MAGENTA 0xF81F
#define COL_ORANGE 0xFD20
#define COL_GRAY 0x8410
#define COL_DARKGREEN 0x0320
#define COL_DARKRED 0x8000
#define COL_NAVY 0x000F
#define COL_TRANS 0x0001 // 透明背景哨兵(近黑, UI 不使用)

class Tft {
public:
  void begin();

  // 帧缓冲绘图 (调用 present() 后真正刷屏)
  void clear(uint16_t color = COL_BLACK);
  void setPixel(int16_t x, int16_t y, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  /// Bresenham 任意斜线 (供曲线/分隔线复用)
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  /// 5x7 字符, scale 整数倍放大
  void drawChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg,
                uint8_t scale = 1);
  /// 字符串(自动大写), bg=0xFFFF 时透明背景(保留 0xFFFF 不参与绘制)
  void drawText(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg,
                uint8_t scale = 1);
  int16_t textWidth(const char *s, uint8_t scale = 1);

  /// 将脏区域刷到屏幕(全帧缓冲)
  void present();
  /// 强制整屏刷新
  void flushAll();

  uint16_t *fb() { return _fb; }

private:
  void hwReset();
  void writeCmd(uint8_t c);
  void writeData(uint8_t d);
  void writeBuf(const uint8_t *p, uint32_t n);
  void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
  void spiByte(uint8_t b);

  void markDirty(int16_t x, int16_t y) {
    if (x < _dx0)
      _dx0 = x;
    if (x > _dx1)
      _dx1 = x;
    if (y < _dy0)
      _dy0 = y;
    if (y > _dy1)
      _dy1 = y;
  }

  static uint16_t _fb[LCD_W * LCD_H];
  int16_t _dx0, _dy0, _dx1, _dy1;
};

extern Tft Lcd;
