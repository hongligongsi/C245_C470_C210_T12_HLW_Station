/**
 * @file nokey.h
 * @brief 无键交互 (开机即用版 -DNO_INPUT 专用): 用"拔插手柄"当作唯一输入
 *
 * 即用版不接编码器/按键, 原来上电就固定 350°C, 待机时间、休眠时间、功率等
 * 参数全都没法改。本模块把手柄在位信号(T12-AD 断线检测)当成一个"按键"。
 *
 * ⚠ 安全前提: 航空头(GX16)严禁带电插拔。所以所有拔插动作都只在
 *   「加热已停 + 烙铁头已冷却(<60°C)」时才被识别, 具体入口为:
 *
 *   断电 -> 拔下手柄 -> 上电: 开 8 秒冷机窗口(强制停热)
 *         -> 这 8 秒内插回手柄 = 进入「无键设置」
 *   休眠  拔插 1 次        -> 唤醒(休眠时加热已停, 属于安全状态)
 *   故障  断电重启         -> 不允许拔插清故障(故障可能来自连接器)
 *
 * 温热状态下拔插一律不认(屏幕提示"冷却后再拔插"), 避免养成带电插拔的习惯。
 *
 * 设置页内(每项 3 秒自动跳到下一项, 始终强制停热, 手柄处于冷态):
 *   快插快拔(插入 <0.9s 就拔出) = 当前项 +1 步
 *   插入并保持(>1.4s)          = 当前项 -1 步
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
  bool armed() const { return _armed; }   // 上电冷机窗口(等用户插手柄进设置)
  bool hotWait() const { return _armed && _hot; } // 窗口内手柄仍烫: 停热等冷却
  uint8_t armLeftSec() const;
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
  bool _armed = false;     // 上电冷机窗口(等插入手柄进设置)
  bool _hot = false;       // 窗口内检出手柄仍烫: 停热等冷却, 不认手势
  float _plugTemp = 0.0f;  // 手柄在位时的最新温度(拔掉后读数不可靠)

  uint8_t _item = 0;
  int16_t _base = 0;    // 进入该项时的原值
  int8_t _count = 0;    // 本项累计步数(可负)
  uint32_t _insertMs = 0;  // 本次插入的时刻(判"快插"还是"保持")
  uint32_t _lastActMs = 0; // 最后一次有效操作(判 3s 自动下一项)
  uint32_t _gestMs = 0;    // 连插手势起点
  uint8_t _tapCount = 0;   // 连插计数
  uint32_t _armMs = 0;     // 冷机窗口起点
  uint32_t _doneMs = 0;    // 完成提示起始

  // 渲染缓存
  uint8_t _lastItem = 0xFF;
  int16_t _lastVal = -32000;
  int16_t _lastBarW = -1;
};

extern Keyless NoKey;
