/**
 * @file calib.h
 * @brief 全屏可视化校准页 (对齐参考机):
 *        实时 Voltage / Current / Power / Temp + 温度功率曲线 + < 值 > 旋钮编辑
 *
 * 交互:
 *   旋转 = 增减校准值
 *   短按 = 保存并退回校准项列表
 *   长按 = 放弃修改并退回
 */
#pragma once
#include <Arduino.h>
#include "menu.h"

class CalibPage {
public:
  void enter(const MenuItem *it); // 从 CALIB 列表选定项后进入
  void exit();                    // 退回列表(不做屏幕清理之外的动作)
  bool active() const { return _item != nullptr; }

  void move(int16_t delta); // 旋转改值
  void select();            // 短按: 保存并退出
  void back();              // 长按: 放弃并退出

  void render(); // 由 Ui 在 Calib 激活时调用(数据有缓存, 可高频调用)

private:
  const MenuItem *_item = nullptr;
  float _ef = 0.0f;
  bool _needsClear = true;

  uint32_t _lastHistMs = 0;
  static constexpr uint8_t HN = 60;
  uint8_t _powHist[HN];
  int16_t _tempHist[HN];
  uint8_t _head = 0;

  // 实时数据缓存(仅变化时重画)
  int16_t _v10 = -1;
  int16_t _c100 = -1;
  int16_t _p10 = -1;
  int16_t _t1 = -2;
  bool _tValid = false;

  char _valCache[14];
  void pushHistory();
};

extern CalibPage Calib;
