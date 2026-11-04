/**
 * @file nokey.h
 * @brief 无键交互 (开机即用版 -DNO_INPUT 专用): 用"拔插手柄"当作唯一输入
 *
 * 即用版不接编码器/按键, 原来上电就固定 350°C, 待机时间、休眠时间、功率等
 * 参数全都没法改。本模块把手柄在位信号(T12-AD 断线检测)当成一个"按键":
 *
 *   平时  连插 3 次(6s 内)  -> 进入「无键设置」, 逐项改参数
 *   故障  拔插 1 次          -> 清除故障
 *   休眠  拔插 1 次          -> 唤醒(解除休眠闩锁)
 *
 * 设置页内(每项 3 秒自动跳到下一项):
 *   快插快拔(插入 <0.7s 就拔出) = 当前项 +1 步
 *   插入并保持(>1.2s)          = 当前项 -1 步
 *   停手 3 秒                  = 保存并进入下一项, 全部走完显示「已保存」
 *
 * 值超过上下限时按步进环绕(如 450 +10 -> 100), 所以加/减都能调到任意档。
 */
#pragma once
#include <Arduino.h>

class Keyless {
public:
  void begin();
  void tick();               // 主循环周期调用(检测拔插手势/推进向导)
  bool active() const { return _active; }
  void render();             // 无键设置页(全屏覆盖)

private:
  int16_t preview() const;   // 当前项的预览值(未提交)
  void commit();             // 提交当前项
  void enter();              // 进入无键设置
  void finish();             // 全部项走完

  bool _active = false;
  bool _finished = false;
  bool _needClear = true;
  bool _lastTip = false;
  bool _holdFired = false;

  uint8_t _item = 0;
  int16_t _base = 0;    // 进入该项时的原值
  int8_t _count = 0;    // 本项累计步数(可负)
  uint32_t _insertMs = 0;  // 本次插入的时刻(判"快插"还是"保持")
  uint32_t _lastActMs = 0; // 最后一次有效操作(判 3s 自动下一项)
  uint32_t _gestMs = 0;    // 连插手势起点
  uint8_t _tapCount = 0;   // 连插计数
  uint32_t _doneMs = 0;    // 完成提示起始

  // 渲染缓存
  uint8_t _lastItem = 0xFF;
  int16_t _lastVal = -32000;
  int16_t _lastBarW = -1;
};

extern Keyless NoKey;
