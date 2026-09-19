#include "station.h"
#include "board.h"

// 内部温度传感器(冷结补偿)在 ddl_config.h 开启 DDL_OTS_ENABLE 后可用,
// 可用 build_flags 加 -DUSE_OTS 启用; 不启用时使用固定 25C 估算。
#ifdef USE_OTS
#include <OnChipTemperature.h>
#endif

Station Stn;

// 上电时标定的电流通道零点(ADC raw)
static float s_zeroCurrRaw = 0.0f;

static float rawToMv(uint16_t raw) {
  return (float)raw * (ADC_VREF_MV / ADC_FULL);
}

// 运行时分压系数: 用 Cfg.vbusK 校准
static inline float vbusDivK() { return (VDIV_RTOP_K + Cfg.vbusK) / Cfg.vbusK; }

const char *Station::tipName() const {
  switch (_tipType) {
  case TIP_T12:
    return "T12";
  case TIP_C210:
    return "C210";
  case TIP_C245:
    return "C245";
  }
  return "T12";
}

void Station::begin() {
  // ---- 模拟引脚 (必须先设置分辨率再 pinMode) ----
  analogReadResolution(12);
  pinMode(PIN_PWR_ADC, INPUT_ANALOG);
  pinMode(PIN_ADC_CURRENT, INPUT_ANALOG);
  pinMode(PIN_T12_AD, INPUT_ANALOG);
  pinMode(PIN_GET_ID, INPUT_ANALOG);

  // ---- 数字输入 ----
  pinMode(PIN_SLEEP, INPUT_PULLUP);
  pinMode(PIN_HLW_SLE, INPUT_PULLUP);
  pinMode(PIN_HLW_SW, INPUT_PULLUP);
#ifdef PIN_VIBRATION
  // 滚珠/震动开关(手柄内弹簧开关, 低有效): T12 震动唤醒用
  pinMode(PIN_VIBRATION, INPUT_PULLUP);
#endif

  // ---- 加热 PWM (TIMA1_CH2, 1kHz 硬件 PWM) ----
  pinMode(PIN_HEATER_PWM, OUTPUT_PWM);
  analogWriteResolution(8);
  analogWrite(PIN_HEATER_PWM, 0);

#ifdef USE_OTS
  ChipTemperature.begin();
#endif

  syncConfig();

  // 上电默认温度取快捷温度1, 限幅
  if (Cfg.quickTemp[0] >= Cfg.tempMin && Cfg.quickTemp[0] <= Cfg.tempMax)
    _setTemp = Cfg.quickTemp[0];
  else
    _setTemp = TEMP_DEFAULT_C;

  _bootMs = millis();
  _lastInputMs = _bootMs;

  // 初始读几次, 让滤波器有初值
  for (uint8_t i = 0; i < 16; i++) {
    uint16_t r = analogRead(PIN_PWR_ADC);
    float v = rawToMv(r) / 1000.0f * vbusDivK();
    _vbus = (_vbus == 0.0f) ? v : (_vbus * 0.5f + v * 0.5f);
    delay(2);
  }

  // 电流零点标定 (上电时加热器必然关闭)
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 32; i++)
    acc += analogRead(PIN_ADC_CURRENT);
  s_zeroCurrRaw = (float)acc / 32.0f;

  // 温度通道零位标定 (冷态, 前 1.5s 在 task() 内继续收敛)
  _zeroRaw = 0.0f;
  acc = 0;
  for (uint8_t i = 0; i < 32; i++)
    acc += analogRead(PIN_T12_AD);
  float z = (float)acc / 32.0f;
  if (z < TIP_RAW_OPEN)
    _zeroRaw = z;
}

void Station::syncConfig() {
  _pidP = Cfg.pidP;
  _pidI = Cfg.pidI;
  _pidD = Cfg.pidD;
  _pidIlimit = Cfg.pidIntegralMax;
  _pidBand = Cfg.pidBand;
  _powerLimitPct = Cfg.powerLimitPct;
  // 钳位当前设定温度到新范围
  if (_setTemp < Cfg.tempMin)
    _setTemp = Cfg.tempMin;
  if (_setTemp > Cfg.tempMax)
    _setTemp = Cfg.tempMax;
}

void Station::setHeater(uint8_t d) {
  _duty = d;
  analogWrite(PIN_HEATER_PWM, d);
}

void Station::adjustSetTemp(int16_t delta) {
  // 步进按 Cfg.tempStep, 方向取 delta 符号
  int16_t step = (int16_t)Cfg.tempStep;
  if (step == 0)
    step = 5;
  int16_t dv = (delta > 0) ? step : ((delta < 0) ? -step : 0);
  int16_t v = (int16_t)_setTemp + dv;
  if (v < Cfg.tempMin)
    v = Cfg.tempMin;
  if (v > Cfg.tempMax)
    v = Cfg.tempMax;
  // 任何调温操作都视为用户输入: 开机并解除休眠
  _powerOn = true;
  _sleeping = false;
  if (v != _setTemp) {
    _setTemp = v;
  }
  _lastInputMs = millis();
}

void Station::togglePower() {
  _powerOn = !_powerOn;
  _lastInputMs = millis();
  _boost = false;
  if (_powerOn) {
    // 开机时清除休眠闩锁
    _sleeping = false;
    _holderSleep = false;
    _sleepSinceMs = 0;
  }
  if (_fault == FAULT_OVERTEMP) {
    _fault = FAULT_NONE; // 手动确认清除超温
  } else if (_fault != FAULT_NONE && _powerOn) {
    _powerOn = false;
  }
}

void Station::clearFault() { _fault = FAULT_NONE; }

void Station::setBoost(bool on) {
  if (on && (!_powerOn || !_tipPresent))
    return;
  _boost = on;
  if (on) {
    _boostStartMs = millis();
    _lastInputMs = _boostStartMs;
  }
}

int16_t Station::cycleQuickTemp() {
  _quickIdx = (_quickIdx + 1) % 3;
  int16_t t = Cfg.quickTemp[_quickIdx];
  if (t < Cfg.tempMin)
    t = Cfg.tempMin;
  if (t > Cfg.tempMax)
    t = Cfg.tempMax;
  _setTemp = t;
  // 短按循环快捷温度: 同时开机并解除休眠
  _powerOn = true;
  _sleeping = false;
  _lastInputMs = millis();
  return t;
}

void Station::wakeFromSleep() {
  _sleeping = false;
  _holderSleep = false;
  _sleepSinceMs = 0;
  _lastInputMs = millis();
}

// ---------------------------------------------------------------------------
// 采样 (5ms)
// ---------------------------------------------------------------------------
void Station::sampleStep(uint32_t now) {
  // --- 输入电压 (4次平均) ---
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 4; i++)
    acc += analogRead(PIN_PWR_ADC);
  float v = rawToMv(acc / 4) / 1000.0f * vbusDivK();
  _vbus = (_vbus == 0.0f) ? v : _vbus * 0.7f + v * 0.3f;

  // --- 电流 (4次平均, 减去上电零位) ---
  acc = 0;
  for (uint8_t i = 0; i < 4; i++)
    acc += analogRead(PIN_ADC_CURRENT);
  float cRaw = (float)(acc / 4) - s_zeroCurrRaw;
  if (cRaw < 3.0f)
    cRaw = 0.0f; // 死区
  float ci = rawToMv((uint16_t)cRaw) / 1000.0f / CURRENT_V_PER_A;
  _current = _current * 0.6f + ci * 0.4f;
  if (_current < 0.02f)
    _current = 0.0f;

  // --- ID 识别电平 ---
  _idRaw = analogRead(PIN_GET_ID);
  _idHigh = _idRaw > 2048;

  // --- 温度: 在一个 PWM 周期内多次采样, 取最低的3个平均 ---
  // (加热时 T12_OUT 被抬高共模电压, 关断相的数据才最干净)
  uint16_t burst[8];
  for (uint8_t i = 0; i < 8; i++) {
    burst[i] = analogRead(PIN_T12_AD);
    delayMicroseconds(200);
  }
  for (uint8_t i = 1; i < 8; i++) // 插入排序
  {
    uint16_t key = burst[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && burst[j] > key) {
      burst[j + 1] = burst[j];
      j--;
    }
    burst[j + 1] = key;
  }
  float tRaw = (burst[0] + burst[1] + burst[2]) / 3.0f;
  _tipRaw = (uint16_t)tRaw;

  // 上电冷态零位继续收敛
  if (now - _bootMs < 1500 && tRaw < TIP_RAW_OPEN) {
    _zeroRaw = (_zeroRaw == 0.0f) ? tRaw : _zeroRaw * 0.85f + tRaw * 0.15f;
  }

  // --- 冷结温度 (芯片内部温度传感器) ---
#ifdef USE_OTS
  if (now - _ambLastMs > 1000) {
    _ambLastMs = now;
    float t;
    if (ChipTemperature.read(t))
      _ambient = _ambient * 0.8f + t * 0.2f;
  }
#endif
}

// ---------------------------------------------------------------------------
// 逻辑/状态 (50ms)
// ---------------------------------------------------------------------------
void Station::logicStep(uint32_t now) {
  // --- 手柄型号识别 (GET_ID 三档: ID接地=C210 / 经10K=T12 / 悬空=C245,
  //     300ms 稳定消抖) ---
  TipType guess = (_idRaw < TIPID_TH_C210)
                      ? TIP_C210
                      : ((_idRaw < TIPID_TH_C245) ? TIP_T12 : TIP_C245);
  if (guess != _tipType) {
    if (guess != _idCandidate) {
      _idCandidate = guess;
      _idCandidateMs = now;
    } else if (now - _idCandidateMs > 300) {
      _tipType = guess;
    }
  } else {
    _idCandidate = guess;
  }

  // --- 烙铁头在位检测 (回差 + 消抖) ---
  bool rawOpen = _tipRaw > TIP_RAW_OPEN;
  if (rawOpen && _tipPresent) {
    if (_noTipSinceMs == 0)
      _noTipSinceMs = now;
    if (now - _noTipSinceMs > 500) {
      _tipPresent = false;
      _boost = false;
    }
  } else if (!rawOpen) {
    if (!_tipPresent && _tipRaw < TIP_RAW_PRESENT)
      _tipPresent = true;
    _noTipSinceMs = 0;
  }

  // --- 支架休眠 (T12 支架 或 HLW 休眠信号) ---
  // 信号持续 2s -> 置 _sleeping 闩锁(停热), 长按编码器解除
  bool slp =
      (digitalRead(PIN_SLEEP) == LOW) || (digitalRead(PIN_HLW_SLE) == LOW);
  if (slp) {
    if (_sleepSinceMs == 0)
      _sleepSinceMs = now;
    if (now - _sleepSinceMs > HOLD_SLEEP_MS) {
      _holderSleep = true;
      _sleeping = true;
    }
  } else {
    _holderSleep = false;
    _sleepSinceMs = 0;
  }

  // --- HLW 换芯开关 ---
  bool sw = digitalRead(PIN_HLW_SW) == LOW;
  if (sw && !_tipSwitchOpen) {
    if (_tipOffSinceMs == 0)
      _tipOffSinceMs = now;
    if (now - _tipOffSinceMs > SW_TIPOFF_MS)
      _tipSwitchOpen = true;
  } else if (!sw) {
    _tipSwitchOpen = false;
    _tipOffSinceMs = 0;
  }

  // --- 升压限时 ---
  if (_boost && now - _boostStartMs > BOOST_MAX_MS)
    _boost = false;

  // --- 电压保护 (200ms 消抖) ---
  if (_vbus > VBUS_OV_FAULT) {
    if (_fault == FAULT_NONE) {
      _fault = FAULT_VOV;
      _faultSinceMs = now;
    }
  } else if (_vbus < VBUS_UV_FAULT && _vbus > 1.0f) {
    if (_fault == FAULT_NONE) {
      _fault = FAULT_VUV;
      _faultSinceMs = now;
    }
  } else if (_fault == FAULT_VOV || _fault == FAULT_VUV) {
    if (now - _faultSinceMs > 1000)
      _fault = FAULT_NONE; // 电压恢复自动清除
  }
  if (_fault == FAULT_VOV || _fault == FAULT_VUV)
    _faultSinceMs = now; // 持续异常则保持

  // --- 过流保护 ---
  if (_current > CURRENT_FAULT_A) {
    if (_fault == FAULT_NONE) {
      _fault = FAULT_OVERCUR;
      _faultSinceMs = now;
    }
  } else if (_fault == FAULT_OVERCUR && now - _faultSinceMs > 2000) {
    _fault = FAULT_NONE;
  }
  if (_fault == FAULT_OVERCUR)
    _faultSinceMs = now;

  // --- 滚珠/震动开关: 晃动手柄视为一次操作, 阻止/退出待机 ---
  // 注: 休眠(_sleeping)只能由长按编码器解除, 震动不清休眠闩锁
  //     待机时间=0 时此开关无意义(245/210 走休眠线)
#ifdef PIN_VIBRATION
  if (Cfg.vibEnable && Cfg.standbyTimeMin > 0) {
    bool vib = digitalRead(PIN_VIBRATION) == LOW;
    if (vib != _lastVibLevel) {
      _lastVibLevel = vib;
      if (!_sleeping)
        _lastInputMs = now;
    }
  }
#endif

  // --- 休眠时间: 进入待机后持续 sleepTimeMin 仍未解除 -> 休眠停热 ---
  if (_mode == MODE_STANDBY && Cfg.sleepTimeMin > 0 && _standbySinceMs != 0 &&
      now - _standbySinceMs > (uint32_t)Cfg.sleepTimeMin * 60000UL) {
    _sleeping = true;
  }

  // --- 冷态零位缓慢跟踪(纠零点漂移) ---
  if (_tipPresent && _duty == 0 && _tipTemp < _ambient + 5.0f) {
    if (_coldSinceMs == 0)
      _coldSinceMs = now;
    if (now - _coldSinceMs > 10000UL && _tipRaw < TIP_RAW_PRESENT)
      _zeroRaw += ((float)_tipRaw - _zeroRaw) * 0.002f;
  } else {
    _coldSinceMs = 0;
  }
}

// ---------------------------------------------------------------------------
// PID 控温 (50ms)
// ---------------------------------------------------------------------------
void Station::controlStep(uint32_t now) {
  // 温度换算
  float vTc = ((float)_tipRaw - _zeroRaw) * (3.3f / ADC_FULL) / TC_AMP_GAIN;
  float dT = vTc * 1000000.0f / TC_SEEBEK_UV_PER_C;
  // 冷结偏移(CJC) + 350° 单点增益(tempCal%, 100=不修正, 作用于热电偶温升)
  float t = _ambient + Cfg.cjcOffset + dT * (Cfg.tempCal * 0.01f);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 600.0f)
    t = 600.0f;
  _tipTemp = _tipTemp * 0.55f + t * 0.45f;

  uint32_t idle = now - _lastInputMs;

  // --- 决定模式与目标温度(优先级重排) ---
  bool wasStandby = (_mode == MODE_STANDBY);
  int16_t target = -1;
  if (_fault != FAULT_NONE) {
    _mode = MODE_FAULT;
  } else if (!_tipPresent) {
    _mode = MODE_OFF;
  } else if (_tipSwitchOpen || !_powerOn) {
    _mode = MODE_OFF;
  } else if (_sleeping) {
    _mode = MODE_SLEEP; // 停热
    target = -1;
  } else if (_boost) {
    _mode = MODE_BOOST;
    int16_t bt = (int16_t)_setTemp + TEMP_BOOST_ADD_C;
    if (bt > Cfg.tempMax)
      bt = Cfg.tempMax;
    target = bt;
  } else if (Cfg.standbyTimeMin > 0 &&
             idle > (uint32_t)Cfg.standbyTimeMin * 60000UL) {
    if (!wasStandby)
      _standbySinceMs = now; // 记录进入待机时刻, 休眠计时从这里起算
    _mode = MODE_STANDBY;    // 待机温度
    target = Cfg.standbyTemp;
  } else {
    _mode = MODE_HEAT;
    target = _setTemp;
  }

  // --- 超温保护(最高优先级) ---
  if (_tipPresent && _tipTemp > TEMP_FAULT_C) {
    _fault = FAULT_OVERTEMP;
    _mode = MODE_FAULT;
    target = -1;
  }
  if (_mode != MODE_STANDBY)
    _standbySinceMs = 0;

  // --- 温度通道合理性: 大功率持续加热却长期无温升 -> 锁故障(手动清除) ---
  // 覆盖热电偶短路到地(零点跟踪会掩盖断线逻辑)、运放损坏、加热 MOS 失效
  if (_duty > SENSOR_NO_RISE_DUTY) {
    if (_heatSinceMs == 0) {
      _heatSinceMs = now;
      _heatStartTemp = _tipTemp;
    } else if (now - _heatSinceMs > SENSOR_NO_RISE_MS &&
               _tipTemp - _heatStartTemp < SENSOR_NO_RISE_C) {
      _fault = FAULT_TIPSENSOR;
      _mode = MODE_FAULT;
      target = -1;
    }
  } else {
    _heatSinceMs = 0; // 占空比回落即重新计时, 保温期断续加热不会误判
  }

  // --- PID ---
  if (target < 0) {
    setHeater(0);
    _integral = 0.0f;
    _prevErr = 0.0f;
    _prevTipTemp = _tipTemp;
    _heatSinceMs = 0;
    return;
  }

  float err = (float)target - _tipTemp;
  float aerr = (err < 0.0f) ? -err : err;
  float out; // 功率% 0..100 (标称 NOMINAL_VBUS 电压下的满功率为基准)
  if (aerr > _pidBand) {
    // 温差大: 全速加热, 积分分离(不累加)
    out = 100.0f;
  } else {
    // 微分先行(derivative on measurement): 微分项只对测量温度求导,
    // 快捷切温/改设定值时 err 突变不会产生微分冲击
    float d = -(_tipTemp - _prevTipTemp);
    float pd = _pidP * err + _pidD * d;
    // 条件积分抗饱和(clamping): 试算后若积分会把输出继续推向饱和, 本轮不累加
    float iNew = _integral + _pidI * err;
    if (iNew > _pidIlimit)
      iNew = _pidIlimit;
    if (iNew < -_pidIlimit)
      iNew = -_pidIlimit;
    float cand = pd + iNew;
    bool saturating =
        (cand >= 100.0f && err > 0.0f) || (cand <= 0.0f && err < 0.0f);
    if (!saturating)
      _integral = iNew;
    out = pd + _integral;
    if (out < 0.0f)
      out = 0.0f;
    if (out > 100.0f)
      out = 100.0f;
  }
  _prevErr = err;
  _prevTipTemp = _tipTemp;

  // 软刹车: 过冲>5C 强制关断并清积分(早于 480C 硬保护)
  if (err < -5.0f) {
    out = 0.0f;
    _integral = 0.0f;
  }

  // 用户功率上限
  out *= (float)_powerLimitPct / 100.0f;
  if (out > 100.0f)
    out = 100.0f;

  // 供电电压前馈: 加热丝功率 P=D*U^2/R, 占空比按 (NOM/Ubus)^2 补偿,
  // 使同一 PID 输出在 19~32V 不同电源下的实际加热功率一致(IronOS/AxxSolder
  // 同款)
  if (_vbus >= 8.0f) {
    float vff = NOMINAL_VBUS / _vbus;
    vff *= vff;
    if (vff > VFF_GAIN_MAX)
      vff = VFF_GAIN_MAX;
    out *= vff;
    if (out > 100.0f)
      out = 100.0f;
  } else {
    out = 0.0f; // 电压读数无效不加热(欠压检测同样会停热)
  }

  uint8_t pwm;
  // 手柄型号功率上限: C210 发热芯细, 钳位 60% 防烧毁(参考固件同款保护);
  // 位于前馈放大之后最终截断, 覆盖积分分离全速段与 BOOST 在内的所有加热输出
  if (_tipType == TIP_C210 && out > C210_POWER_LIMIT_PCT)
    out = C210_POWER_LIMIT_PCT;
  pwm = (uint8_t)(out * (float)PWM_MAX / 100.0f + 0.5f);
  setHeater(pwm);
}

void Station::task() {
  uint32_t now = millis();

  if (now - _lastSampleMs >= 5) {
    _lastSampleMs = now;
    sampleStep(now);
  }
  if (now - _lastLogicMs >= 50) {
    _lastLogicMs = now;
    logicStep(now);
  }
  if (now - _lastControlMs >= 50) {
    _lastControlMs = now;
    controlStep(now);
  }
}
