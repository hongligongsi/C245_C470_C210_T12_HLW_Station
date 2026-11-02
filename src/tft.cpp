/**
 * @file tft.h
 * @brief 软件 SPI 彩屏驱动实现
 */
#include "tft.h"
#include "board.h"
#include "font5x7.h"
#include "font_cn.h"
#include <hc32_ddl.h>

// 屏幕物理偏移 (不同玻璃批次可能不同, 可在 platformio.ini 覆盖)
#if defined(LCD_CTRL_ST7735)
#ifndef LCD_X_OFFSET
#define LCD_X_OFFSET 26
#endif
#ifndef LCD_Y_OFFSET
#define LCD_Y_OFFSET 1
#endif
#ifndef LCD_MADCTL
#define LCD_MADCTL 0x00 // 竖屏; 若画面方向/颜色异常可试 0xC0 / 0xA0
#endif
#else
#if LCD_TYPE == 1
#ifndef LCD_X_OFFSET
#define LCD_X_OFFSET 52
#endif
#ifndef LCD_Y_OFFSET
#define LCD_Y_OFFSET 0
#endif
#else
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0
#endif
#ifndef LCD_MADCTL
#if LCD_TYPE == 3
// 横屏: MV=行列交换; 若显示方向颠倒/镜像可改 0xA0 / 0xE0 / 0x20 (build_flags
// 覆盖)
#define LCD_MADCTL 0x60 // MX | MV
#else
#define LCD_MADCTL 0x00
#endif
#endif
#endif

#ifndef LCD_SPI_TICKS
#define LCD_SPI_TICKS 1
#endif

Tft Lcd;
uint16_t Tft::_fb[LCD_W * LCD_H];

// --- 直接操作 GPIO 置位/复位寄存器(POSR/PORR), 获得最快软件 SPI ---
// PA15=SCLK(PortA), PB4=SDA(PortB); 写 1 置位/复位, 不影响其它引脚
#define SCLK_H() (M4_PORT->POSRA = (uint16_t)(M4_PORT->POSRA | (1u << 15)))
#define SCLK_L() (M4_PORT->PORRA = (uint16_t)(M4_PORT->PORRA | (1u << 15)))
#define SDA_H() (M4_PORT->POSRB = (uint16_t)(M4_PORT->POSRB | (1u << 4)))
#define SDA_L() (M4_PORT->PORRB = (uint16_t)(M4_PORT->PORRB | (1u << 4)))
// CS/DC 翻转频率低, 直接用 Arduino digitalWrite
#define CS_H() digitalWrite(PIN_TFT_CS, HIGH)
#define CS_L() digitalWrite(PIN_TFT_CS, LOW)
#define DC_H() digitalWrite(PIN_TFT_DC, HIGH)
#define DC_L() digitalWrite(PIN_TFT_DC, LOW)

static inline void spiDelay() {
#if LCD_SPI_TICKS >= 2
  __NOP();
  __NOP();
#endif
#if LCD_SPI_TICKS >= 3
  __NOP();
  __NOP();
  __NOP();
  __NOP();
#endif
}

void Tft::spiByte(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80)
      SDA_H();
    else
      SDA_L();
    spiDelay();
    SCLK_H();
    spiDelay();
    SCLK_L();
    b <<= 1;
  }
}

void Tft::writeCmd(uint8_t c) {
  DC_L();
  spiByte(c);
}

void Tft::writeData(uint8_t d) {
  DC_H();
  spiByte(d);
}

void Tft::writeBuf(const uint8_t *p, uint32_t n) {
  DC_H();
  while (n--)
    spiByte(*p++);
}

void Tft::hwReset() {
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(20);
  digitalWrite(PIN_TFT_RST, LOW);
  delay(50);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(120);
}

void Tft::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
#ifdef LCD_LANDSCAPE
  // 横屏(MV=1): 帧缓冲 x(0..319)->GRAM 行地址(0x2B), y(0..239)->列地址(0x2A)
  uint16_t c0 = (uint16_t)(y0 + LCD_Y_OFFSET);
  uint16_t c1 = (uint16_t)(y1 + LCD_Y_OFFSET);
  uint16_t r0 = (uint16_t)(x0 + LCD_X_OFFSET);
  uint16_t r1 = (uint16_t)(x1 + LCD_X_OFFSET);
#else
  uint16_t c0 = (uint16_t)(x0 + LCD_X_OFFSET);
  uint16_t c1 = (uint16_t)(x1 + LCD_X_OFFSET);
  uint16_t r0 = (uint16_t)(y0 + LCD_Y_OFFSET);
  uint16_t r1 = (uint16_t)(y1 + LCD_Y_OFFSET);
#endif
  writeCmd(0x2A); // 列地址
  writeData(c0 >> 8);
  writeData(c0 & 0xFF);
  writeData(c1 >> 8);
  writeData(c1 & 0xFF);
  writeCmd(0x2B); // 行地址
  writeData(r0 >> 8);
  writeData(r0 & 0xFF);
  writeData(r1 >> 8);
  writeData(r1 & 0xFF);
  writeCmd(0x2C);
}

void Tft::begin() {
  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_TFT_CLK, OUTPUT);
  pinMode(PIN_TFT_SDA, OUTPUT);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT);
  pinMode(PIN_TFT_LED, OUTPUT);

  CS_H();
  SCLK_L();
  digitalWrite(PIN_TFT_LED, HIGH);

  hwReset();
  CS_L();

  writeCmd(0x01); // soft reset
  delay(120);
  writeCmd(0x11); // sleep out
  delay(120);

#ifdef LCD_CTRL_ST7735
  writeCmd(0x3A);
  writeData(0x05); // 16bit/pixel
  writeCmd(0x36);
  writeData(LCD_MADCTL); // 方向/色彩序
  writeCmd(0x21);        // 反色显示
  writeCmd(0x13);        // normal display on
#else
  // ST7789 通用最小初始化
  writeCmd(0x3A);
  writeData(0x05);
  writeCmd(0x36);
  writeData(LCD_MADCTL);
  writeCmd(0x21);
  writeCmd(0x13);
#endif

  writeCmd(0x29); // display on
  delay(20);
  CS_H();

  _dx0 = LCD_W;
  _dy0 = LCD_H;
  _dx1 = -1;
  _dy1 = -1;
  clear(COL_BLACK);
  flushAll();
}

void Tft::clear(uint16_t color) {
  for (uint32_t i = 0; i < (uint32_t)LCD_W * LCD_H; i++)
    _fb[i] = color;
  _dx0 = 0;
  _dy0 = 0;
  _dx1 = LCD_W - 1;
  _dy1 = LCD_H - 1;
}

void Tft::setPixel(int16_t x, int16_t y, uint16_t color) {
  if ((uint16_t)x >= LCD_W || (uint16_t)y >= LCD_H)
    return;
  _fb[y * LCD_W + x] = color;
  markDirty(x, y);
}

void Tft::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t j = 0; j < h; j++) {
    int16_t yy = y + j;
    if (yy < 0 || yy >= LCD_H)
      continue;
    for (int16_t i = 0; i < w; i++) {
      int16_t xx = x + i;
      if (xx < 0 || xx >= LCD_W)
        continue;
      _fb[yy * LCD_W + xx] = color;
    }
  }
  int16_t x0 = max(x, 0), y0 = max(y, 0);
  int16_t x1 = min(x + w - 1, LCD_W - 1), y1 = min(y + h - 1, LCD_H - 1);
  if (x1 >= x0 && y1 >= y0) {
    if (x0 < _dx0)
      _dx0 = x0;
    if (y0 < _dy0)
      _dy0 = y0;
    if (x1 > _dx1)
      _dx1 = x1;
    if (y1 > _dy1)
      _dy1 = y1;
  }
}

void Tft::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void Tft::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

void Tft::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color) {
  int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = dx - dy;
  for (;;) {
    setPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int16_t e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Tft::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawHLine(x, y, w, color);
  drawHLine(x, y + h - 1, w, color);
  drawVLine(x, y, h, color);
  drawVLine(x + w - 1, y, h, color);
}

void Tft::drawChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg,
                   uint8_t scale) {
  const uint8_t *g = fontGlyph(ch);
  for (uint8_t col = 0; col < FONT_W; col++) {
    uint8_t bits = g[col];
    for (uint8_t row = 0; row < FONT_H; row++) {
      bool on = (bits >> row) & 1u;
      if (!on && bg == COL_TRANS)
        continue;
      fillRect(x + col * scale, y + row * scale, scale, scale, on ? fg : bg);
    }
  }
  // 字符右侧 1 列间隔 (透明时无需擦除)
  if (bg != COL_TRANS)
    fillRect(x + FONT_W * scale, y, scale, FONT_H * scale, bg);
}

// UTF-8 解码: 成功返回 true 并写入码点, 字符串结束返回 false
static bool utf8Decode(const char *&p, uint32_t &cp) {
  uint8_t b = (uint8_t)*p;
  if (b == 0)
    return false;
  if (b < 0x80) {
    cp = b;
    p++;
    return true;
  }
  uint8_t n;
  if ((b & 0xE0) == 0xC0) {
    cp = b & 0x1F;
    n = 1;
  } else if ((b & 0xF0) == 0xE0) {
    cp = b & 0x0F;
    n = 2;
  } else if ((b & 0xF8) == 0xF0) {
    cp = b & 0x07;
    n = 3;
  } else { // 非法首字节, 跳过
    p++;
    cp = 0xFFFD;
    return true;
  }
  p++;
  for (uint8_t i = 0; i < n; i++) {
    if (((uint8_t)*p & 0xC0) != 0x80) {
      cp = 0xFFFD;
      return true;
    }
    cp = (cp << 6) | ((uint8_t)*p & 0x3F);
    p++;
  }
  return true;
}

// 12x12 汉字: 逐行按连续段填充, 未收录的字符留空; cs 为整数放大倍数
void Tft::drawCnChar(int16_t x, int16_t y, uint32_t cp, uint16_t fg,
                     uint16_t bg, uint8_t cs) {
  const CnGlyph *g = fontCn(cp);
  if (!g)
    return;
  for (uint8_t r = 0; r < CN_H; r++) {
    uint16_t bits = g->row[r];
    uint8_t c = 0;
    while (c < CN_W) {
      bool on = (bits >> (15 - c)) & 1u;
      uint8_t n = 1;
      while (c + n < CN_W &&
             (((bits >> (15 - c - n)) & 1u) == (on ? 1u : 0u)))
        n++;
      if (on || bg != COL_TRANS)
        fillRect(x + c * cs, y + r * cs, (int16_t)n * cs, cs, on ? fg : bg);
      c += n;
    }
  }
  if (bg != COL_TRANS) // 字间 1px 间隔(放大后同步为 cs 像素)
    fillRect(x + CN_W * cs, y, cs, CN_H * cs, bg);
}

void Tft::drawText(int16_t x, int16_t y, const char *s, uint16_t fg,
                   uint16_t bg, uint8_t scale, uint8_t cnScale) {
  int16_t lineH = (int16_t)(FONT_H * scale);
  int16_t cjkH = (int16_t)(CN_H * cnScale);
  // 默认(cnScale=1): 汉字固定 12px, 相对 ASCII 行高居中(保持旧布局不变)
  int16_t ay = y;
  int16_t cy = y + (lineH - cjkH) / 2;
  if (cnScale > 1 && cjkH > lineH) { // 放大汉字时以汉字行高为准, ASCII 居中
    ay = y + (cjkH - lineH) / 2;
    cy = y;
  }
  int16_t cx = x;
  uint32_t cp;
  while (utf8Decode(s, cp)) {
    if (cp < 0x80) {
      char ch = (char)cp;
      if (ch >= 'a' && ch <= 'z')
        ch -= 32;
      drawChar(cx, ay, ch, fg, bg, scale);
      cx += (FONT_W + 1) * scale;
    } else {
      drawCnChar(cx, cy, cp, fg, bg, cnScale);
      cx += CN_ADV * cnScale;
    }
  }
}

int16_t Tft::textWidth(const char *s, uint8_t scale, uint8_t cnScale) {
  int16_t w = 0;
  uint32_t cp;
  while (utf8Decode(s, cp)) {
    if (cp < 0x80)
      w += (FONT_W + 1) * scale;
    else
      w += CN_ADV * cnScale;
  }
  return w;
}

void Tft::present() {
  if (_dx1 < _dx0 || _dy1 < _dy0)
    return;

  CS_L();
  setWindow(_dx0, _dy0, _dx1, _dy1);
  DC_H();
  for (int16_t y = _dy0; y <= _dy1; y++) {
    const uint16_t *row = &_fb[y * LCD_W + _dx0];
    uint16_t n = _dx1 - _dx0 + 1;
    while (n--) {
      uint16_t c = *row++;
      spiByte(c >> 8);
      spiByte(c & 0xFF);
    }
  }
  CS_H();

  _dx0 = LCD_W;
  _dy0 = LCD_H;
  _dx1 = -1;
  _dy1 = -1;
}

void Tft::flushAll() {
  _dx0 = 0;
  _dy0 = 0;
  _dx1 = LCD_W - 1;
  _dy1 = LCD_H - 1;
  present();
}
