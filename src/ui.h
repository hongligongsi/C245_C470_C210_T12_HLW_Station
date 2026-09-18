/**
 * @file ui.h
 * @brief 屏幕界面: 主页(标准/曲线) + 故障页 + 菜单渲染 + 主题调色板
 *        控件仅在数值变化时重绘, 配合脏区域把软 SPI 流量降到最低
 */
#pragma once
#include <Arduino.h>

// 主题调色板: 所有界面颜色取自此处, 切换主题即时生效
struct Palette {
  uint16_t bg;     // 背景
  uint16_t fg;     // 普通文字
  uint16_t accent; // 强调(设定温度/数值)
  uint16_t ok;     // 就绪/正常
  uint16_t warn;   // 加热/警告
  uint16_t grid;   // 分隔线/次要
  uint16_t danger; // 故障/无头
  uint16_t info;   // 信息(休眠/遥测)
};

extern Palette Pal;

class Ui {
public:
  void begin();
  void update();     // 主循环周期调用(内部 100ms 节流)
  void invalidate(); // 强制下一帧整屏重绘(退出菜单/切主题时)
  void showQuickTempPrompt(int16_t temp); // 主页短按循环快捷温度时调用

private:
  void drawStaticFrame(bool curve);
  void drawStatus();
  void drawBigTemp();
  void drawSetTemp();
  void drawDutyBar();
  void drawTelemetry(int16_t y1, int16_t y2);
  void drawFooter();
  void drawFault();
  void drawCurve();    // 曲线风格主页(小屏)
  void drawStandard(); // 标准风格主页(小屏)
  void pushHistory();

  // --- 320x240 宽屏布局 (LCD_W>=200) ---
  void drawWideFrame(bool curve);
  void drawWideTip();
  void drawWideVIP();
  void drawWideCh();
  void drawWideBig();
  void drawWideSet();
  void drawWideCurve(); // 双轴刻度网格: 绿=功率% 蓝=温度
  void drawWideBar();   // 标准风格: 大功率条
  uint32_t _lastMs = 0;
  uint32_t _lastHistMs = 0;
  bool _histDirty = true; // 曲线需重绘

  // 曲线历史环形缓冲(宽屏 64 点, 小屏 48 点)
#if LCD_W >= 200
  static constexpr uint8_t HIST_N = 64;
#else
  static constexpr uint8_t HIST_N = 48;
#endif
  uint8_t _powHist[HIST_N];
  int16_t _tempHist[HIST_N];
  uint8_t _histHead = 0;

  // 快捷温度切换提示
  uint32_t _quickPromptMs = 0;
  int16_t _quickPromptTemp = 0;

  // 各控件缓存, 仅在数值变化时重绘(减少软 SPI 传输量)
  int8_t _lastMode = -1;
  int16_t _lastBigTemp = 0x7FFF;
  bool _lastBigValid = false;
  uint8_t _lastBigColor = 0;
  int16_t _lastSetTemp = -1;
  uint8_t _lastDuty = 0xFF;
  int16_t _lastVBus10 = -1;
  int16_t _lastCurr10 = -1;
  int16_t _lastCurr100 = -1; // 宽屏: 0.01A
  int16_t _lastPwr10 = -1;   // 宽屏: 0.1W
  uint8_t _lastTipType = 0xFF;
  int16_t _lastCh[3] = {-1, -1, -1};
  uint8_t _lastQuickIdx = 0xFF;
  int8_t _lastAmbient = -100;
  uint8_t _lastDutyPct = 0xFF;
  uint8_t _lastFooter = 0xFF;
  uint8_t _lastPage = 0xFF; // 0=home 1=fault (菜单由 MenuCtl 自管)
  uint8_t _lastStyle = 0xFF;
  uint8_t _lastTheme = 0xFF;
};

extern Ui Screen;
