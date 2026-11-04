/**
 * @file station.h
 * @brief 焊台核心: ADC采样 / 热电偶换算 / PID控温 / 休眠换芯 / 电压电流保护
 */
#pragma once
#include "board.h"
#include "settings.h"
#include <Arduino.h>

enum WorkMode : uint8_t {
  MODE_OFF = 0, // 关机(不加热)
  MODE_HEAT,    // 正常加热
  MODE_SLEEP,   // 支架休眠
  MODE_STANDBY, // 无操作待机
  MODE_BOOST,   // 一键升温
  MODE_FAULT,   // 故障锁定
};

enum FaultCode : uint8_t {
  FAULT_NONE = 0,
  FAULT_NOTIP,     // 未检测到烙铁头
  FAULT_VUV,       // 输入欠压
  FAULT_VOV,       // 输入过压
  FAULT_OVERTEMP,  // 温度超限
  FAULT_OVERCUR,   // 过流
  FAULT_TIPSENSOR, // 温度信号异常
  FAULT_VTIP,      // 供电电压与手柄型号不匹配 (24V/48V)
};

// GET_ID 识别的手柄型号
enum TipType : uint8_t {
  TIP_T12 = 0,
  TIP_C210,
  TIP_C245,
};

class Station {
public:
  void begin();

  // 从 Cfg 重新注入运行时参数(菜单改 PID/功率/温度范围后调用)
  void syncConfig();

  // 主循环周期调用
  void task();

  // 用户操作
  void adjustSetTemp(int16_t delta);
  // 直接设定目标温度(无键设置页用), 不改变开关机/休眠状态
  void setSetTemp(int16_t t);
  // 无键设置期间锁定加热(=true 时强制 MODE_OFF 并清空积分), 防止边设置边烧红
  void setConfigLock(bool on);
  void togglePower();
  void setBoost(bool on);
  int16_t cycleQuickTemp(); // 循环快捷温度, 返回新设定温度
  void wakeFromSleep();
  void clearFault();
  void notifyInput() { _lastInputMs = millis(); }

  // 状态(供 UI)
  WorkMode mode() const { return _mode; }
  FaultCode fault() const { return _fault; }
  float tipTemp() const { return _tipTempAvg; }   // 10 点窗口平滑值(显示/保护)
  float tipTempFast() const { return _tipTemp; }  // 快速滤波值(控制环内部使用)
  float ambient() const { return _ambient; }
  int16_t setTemp() const { return _setTemp; }
  uint8_t duty() const { return _duty; }
  float vbus() const { return _vbus; }
  float current() const { return _current; }
  float power() const { return _vbus * _current; } // 实时功率 W=U*I
  bool tipPresent() const { return _tipPresent; }
  TipType tipType() const { return _tipType; }
  const char *tipName() const;
  uint8_t quickIdx() const { return _quickIdx; }
  uint16_t idRaw() const { return _idRaw; }
  bool boostActive() const { return _boost; }
  bool holderSleep() const { return _holderSleep; }
  bool tipSwitchOpen() const { return _tipSwitchOpen; }
  bool idHigh() const { return _idHigh; }
  bool hvMode() const { return _hvMode; } // true=C470 状态(48V)
  bool hardOff() const { return _hardOff; }       // 470°C 绝对强关中
  float tempMaxEff() const { return _tempMaxEff; } // 当前手柄允许的最高设定温度

private:
  // 采样与控制
  void sampleStep(uint32_t now);
  void controlStep(uint32_t now);
  void logicStep(uint32_t now);
  void setHeater(uint8_t d);
  void checkVoltProfile(uint32_t now);

  // 按识别出的手柄型号装载 PID / 功率上限 / 温度上限
  // (对应原厂 0x1F68C 参数装载链 + 类型参数表)
  void applyTipProfile();
  // ADC 原始值 → 温度: 运放电压 → 热电偶电动势 → K 型分度表 + 冷端补偿
  float rawToTemp(float raw) const;

  // 调温参数(离散PID, 50ms): 由 Cfg 注入, 运行时可调
  float _pidP = 25.0f;
  float _pidI = 0.3f;
  float _pidD = 15.0f;
  float _pidIlimit = 120.0f;
  float _pidBand = 25.0f;      // |err|>band 全速加热且不积分(积分分离)
  uint8_t _powerLimitPct = 70; // 功率上限 %

  int16_t _setTemp = TEMP_DEFAULT_C;
  float _tipTemp = 25.0f;    // 快速滤波值(控制环)
  float _tipTempAvg = 25.0f; // 10 点窗口平滑值(显示/超温判定)
  float _ambient = 25.0f;
  float _vbus = 0.0f;
  float _current = 0.0f;
  uint16_t _tipRaw = 0;
  // 10 点滑动窗口: 对应原厂 0x65E8 环形写入 + 0xAAE0 求和÷10
  // (原厂按 5ms 节拍写入, 本工程同节拍, 窗口 ≈ 50ms)
  uint16_t _win[10] = {0};
  uint8_t _winIdx = 0;
  uint8_t _winCount = 0;
  uint32_t _winSum = 0;
  float _tempMaxEff = TEMP_MAX_C; // 全局上限与手柄上限取小
  bool _hardOff = false;          // 470°C 绝对强关标志
  uint16_t _idRaw = 0;
  float _zeroRaw = 0.0f;
  bool _idHigh = true;
  TipType _tipType = TIP_T12;     // GET_ID 消抖后的手柄型号
  TipType _idCandidate = TIP_T12; // 待确认型号
  uint32_t _idCandidateMs = 0;
  bool _hvMode = false;           // C470 状态(悬空 ID + 48V 输入)
  float _nominalV = NOMINAL_VBUS; // 电压前馈标称, 245/470 状态自动切换

  bool _tipPresent = false;
  bool _holderSleep = false;
  bool _tipSwitchOpen = false;
  bool _boost = false;
  bool _powerOn = false;
  bool _cfgLock = false;     // 无键设置锁定(强制停热)
  bool _sleeping = false;    // 休眠闩锁(支架或休眠时间到期)
  bool _lastVibLevel = true; // 滚珠开关上次电平(低有效, 上拉)
  uint8_t _quickIdx = 0;

  WorkMode _mode = MODE_OFF;
  FaultCode _fault = FAULT_NONE;

  uint8_t _duty = 0;
  float _integral = 0.0f;
  float _prevErr = 0.0f;
  float _prevTipTemp = 25.0f; // 上周期测温(微分先行: 只对测量值微分)

  // 加热无温升检测(温度通道合理性)
  uint32_t _heatSinceMs = 0;   // 持续大功率加热起始时刻, 0=未在加热
  float _heatStartTemp = 0.0f; // 该段加热起始温度

  // 计时/防抖
  uint32_t _lastSampleMs = 0;
  uint32_t _lastControlMs = 0;
  uint32_t _lastLogicMs = 0;
  uint32_t _lastInputMs = 0;
  uint32_t _sleepSinceMs = 0;
  uint32_t _standbySinceMs = 0;
  uint32_t _tipOffSinceMs = 0;
  uint32_t _noTipSinceMs = 0;
  uint32_t _boostStartMs = 0;
  uint32_t _faultSinceMs = 0;
  uint32_t _coldSinceMs = 0;
  uint32_t _bootMs = 0;
  uint32_t _ambLastMs = 0;
};

extern Station Stn;
