/**
 * @file ui.h
 * @brief 屏幕界面: 主页(标准/曲线) + 故障页 + 菜单渲染 + 主题调色板
 *        控件仅在数值变化时重绘, 配合脏区域把软 SPI 流量降到最低
 */
#pragma once
#include <Arduino.h>

// 主题调色板: 所有界面颜色取自此处, 切换主题即时生效
struct Palette {
  uint16_t bg;      // 背景
  uint16_t panel;   // 顶栏/激活胶囊底色
  uint16_t fg;      // 普通文字
  uint16_t accent;  // 强调(设定温度/数值)
  uint16_t ok;      // 就绪/正常
  uint16_t warn;    // 加热/警告
  uint16_t grid;    // 主分隔线/刻度
  uint16_t gridMin; // 次级网格线
  uint16_t danger;  // 故障/无头
  uint16_t info;    // 信息(休眠/遥测)
  uint16_t vCol;    // 电压语义色
  uint16_t cCol;    // 电流语义色
  uint16_t pCol;    // 功率语义色
  uint16_t pill;    // 激活通道胶囊底
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
  void drawSmallTip(); // 小屏顶栏左侧: 动态手柄型号
  void drawBigTemp();
  void drawSetTemp();
  void drawDutyBar();
  void drawTelemetry(int16_t y1, int16_t y2);
  void drawFooter();
  void drawFault();
  // 全屏状态页: 无工具 / 换芯 / 休眠 (互斥, 覆盖主页)
  void drawStatePage(const char *title, uint16_t titleCol, const char *sub,
                     const char *hint);
  void drawNoTipPage();
  void drawTipChangePage();
  void drawSleepPage();     // 静态部分: 标题/副标题
  void drawSleepTemp();     // 动态部分: 当前温度(变化才重绘)
  void drawCurve();    // 曲线风格主页(小屏)
  void drawStandard(); // 标准风格主页(小屏)
  void pushHistory();

  // --- 320x240 宽屏布局 (LCD_W>=200) ---
  void drawWideFrame(bool info); // true=信息版(STYLE_CURVE), false=简洁版(STYLE_BAR)
  void drawWideTip();
  void drawWideIcons();  // 顶栏状态图标: 沙漏/月亮/喇叭
  void drawWideStatus(); // 顶栏状态提示文字(重启恢复/换芯中/短路过载...) 
  // 信息版(STYLE_CURVE)
  void drawWideInfoVIP();
  void drawWideInfoCh();
  void drawWideInfoBig();
  void drawWideInfoSet();
  void drawWideCurve(); // 双轴刻度网格: 绿=功率% 蓝=温度
  // 简洁版(STYLE_BAR)
  void drawWideBar();   // 左侧竖向渐变功率条
  void drawWideBoxes(); // 底部三个圆角快捷温度框
  void drawWideBig();   // 居中大字温度
  void drawWideSet();   // 居中 SET 行
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
  uint8_t _lastHdrIcons = 0xFF; // 宽屏顶栏图标状态缓存
  uint8_t _lastStatus = 0xFF;   // 宽屏顶栏状态文字缓存
  uint32_t _bootMs = 0;         // 开机时刻(前 1.5s 显示"重启恢复")
  uint32_t _tipChgMs = 0;       // 换芯时刻(1.2s 内显示"换芯中")
  int16_t _lastCh[3] = {-1, -1, -1};
  uint8_t _lastQuickIdx = 0xFF;
  int8_t _lastAmbient = -100;
  uint8_t _lastDutyPct = 0xFF;
  uint8_t _lastFooter = 0xFF;
  uint8_t _lastNoTipKey = 0xFF; // 无工具页(即用版)文案缓存: 冷机窗口倒计时/冷却提示
  uint8_t _lastPage = 0xFF; // 0=home 1=fault 2=无工具 3=换芯 4=休眠
  int16_t _lastSleepTemp = 0x7FFF;
  uint8_t _lastStyle = 0xFF;
  uint8_t _lastTheme = 0xFF;
};

extern Ui Screen;
