/**
 * @file about.h
 * @brief 全屏「关于本机」信息页: 固件版本 / 编译时间 / 主控 / 手柄 / 电源 / 运行时长
 *
 * 交互(只读页):
 *   旋转 = 无操作
 *   短按 = 返回菜单
 *   长按 = 返回菜单
 */
#pragma once
#include <Arduino.h>

class AboutPage {
public:
  void enter();
  void exit();
  bool active() const { return _active; }

  void move(int16_t /*delta*/) {} // 只读页, 旋转无操作
  void select();                  // 短按: 返回
  void back() { exit(); }         // 长按: 返回

  void render(); // 由 Ui 在 About 激活时调用(变化行才重绘)

private:
  bool _active = false;
  bool _needsClear = true;

  // 行内容缓存(仅变化行重绘, 降低软 SPI 流量)
  static constexpr uint8_t ROW_N = 9;
  char _rows[ROW_N][24];
};

extern AboutPage About;
