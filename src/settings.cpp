/**
 * @file settings.cpp
 * @brief 设置持久化实现: 片内 Flash 末扇区
 */
#include "settings.h"
#include "board.h"
#include <hc32_ddl.h>
#include <string.h>

// 全局实例
Settings_t Cfg;

// ---------------------------------------------------------------------------
// CRC8 (多项式 0x07, 初始 0xFF)
// ---------------------------------------------------------------------------
static uint8_t crc8(const uint8_t *p, size_t n) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (uint8_t b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }
  return crc;
}

// ---------------------------------------------------------------------------
// 默认值
// ---------------------------------------------------------------------------
static void fillDefaults() {
  memset(&Cfg, 0, sizeof(Cfg));
  Cfg.magic = SETTINGS_MAGIC;
  Cfg.version = SETTINGS_VERSION;

  // 基本
  Cfg.soundOn = 1;
  Cfg.brightness = 10; // 亮度 1~10 档(每档 10%)
  Cfg.tempMin = TEMP_MIN_C;
  Cfg.tempMax = TEMP_MAX_C;
  Cfg.tempStep = 5;

  // 工具
  Cfg.standbyTemp = TEMP_STANDBY_C;
  Cfg.standbyTimeMin = 0; // 无震动开关, 禁用
#ifdef NO_INPUT
  Cfg.sleepTimeMin = 5; // 开机即用版(无输入): 5 分钟后停热
#else
  Cfg.sleepTimeMin = 10; // 其余版本默认 10 分钟, 菜单可改
#endif
  Cfg.quickTemp[0] = 300;
  Cfg.quickTemp[1] = 350;
  Cfg.quickTemp[2] = 400;
  Cfg.boostOn = 0;
  Cfg.vibEnable = 0; // V1.7 板未引出震动开关, 默认关; 需配合非0待机时间
  // PID 默认(用户规格: 25 / 0.3 / 15)
  Cfg.pidP = 25.0f;
  Cfg.pidI = 0.3f;
  Cfg.pidD = 15.0f;
  Cfg.pidBand = 25.0f;
  Cfg.pidIntegralMax = 120.0f;
  Cfg.powerLimitPct = 70;

  // 手柄参数表(对应原厂"按手柄型号装载 PID 与限值"的类型表):
  //   PID 三型默认相同(沿用 25/0.3/15 的实测参数), 换芯后仍可各自微调
  //   功率上限: C210 发热芯细 -> 60%(0 表示跟随全局)
  //   温度上限: C210 细芯长期高温易烧断 -> 400°C(0 表示跟随全局)
  Cfg.pidFollowTip = 1;
  for (uint8_t i = 0; i < TIP_TYPE_COUNT; i++) {
    Cfg.tipPowerPct[i] = 0;
    Cfg.tipTempMax[i] = 0;
    Cfg.tipPid[i][0] = 25.0f;
    Cfg.tipPid[i][1] = 0.3f;
    Cfg.tipPid[i][2] = 15.0f;
  }
  Cfg.tipPowerPct[TIP_C210_IDX] = 60;
  Cfg.tipTempMax[TIP_C210_IDX] = 400;

  // 主题
  Cfg.style = STYLE_STANDARD; // 默认简洁版, 菜单可切换信息版
  Cfg.theme = THEME_DARK;

  // 校准
  Cfg.vbusK = VDIV_RBOT_K;
  Cfg.cjcOffset = CJC_OFFSET_C;
  Cfg.tempCal = 100.0f; // 350° 单点增益, 100%=不修正
}

// "重置配置": 恢复基本+工具到默认, 保留主题/校准
static void fillPartialDefaults() {
  uint8_t style = Cfg.style;
  uint8_t theme = Cfg.theme;
  float vbusK = Cfg.vbusK;
  float cjcOffset = Cfg.cjcOffset;
  float tempCal = Cfg.tempCal;
  fillDefaults();
  Cfg.style = style;
  Cfg.theme = theme;
  Cfg.vbusK = vbusK;
  Cfg.cjcOffset = cjcOffset;
  Cfg.tempCal = tempCal;
}

// ---------------------------------------------------------------------------
// 公开接口
// ---------------------------------------------------------------------------
bool Settings_Load() {
  const Settings_t *flash = (const Settings_t *)SETTINGS_FLASH_ADDR;
  memcpy(&Cfg, flash, sizeof(Cfg));

  if (Cfg.magic != SETTINGS_MAGIC || Cfg.version != SETTINGS_VERSION) {
    fillDefaults();
    return false;
  }
  uint8_t c = crc8((const uint8_t *)&Cfg, offsetof(Settings_t, crc));
  if (c != Cfg.crc) {
    fillDefaults();
    return false;
  }
  // 字段范围钳位, 防止 Flash 半损坏导致越界
  if (Cfg.brightness == 0)
    Cfg.brightness = 10;
  else if (Cfg.brightness > 10) // 旧固件 1~100% 迁移为 10 档
    Cfg.brightness = (uint8_t)((Cfg.brightness + 9) / 10);
  if (Cfg.tempStep == 0)
    Cfg.tempStep = 5;
  if (Cfg.powerLimitPct == 0)
    Cfg.powerLimitPct = 70;
  if (Cfg.pidBand < 1.0f)
    Cfg.pidBand = 25.0f;
  if (Cfg.pidIntegralMax < 1.0f)
    Cfg.pidIntegralMax = 120.0f;
  // 校准值越界保护(避免分压系数为 0 导致除零)
  if (Cfg.vbusK < 0.5f || Cfg.vbusK > 50.0f)
    Cfg.vbusK = VDIV_RBOT_K;
  if (Cfg.cjcOffset < -30.0f || Cfg.cjcOffset > 30.0f)
    Cfg.cjcOffset = CJC_OFFSET_C;
  if (Cfg.tempCal < 80.0f || Cfg.tempCal > 120.0f)
    Cfg.tempCal = 100.0f;
  if (Cfg.tempMax > TEMP_MAX_C)
    Cfg.tempMax = TEMP_MAX_C;
  if (Cfg.tempMin >= Cfg.tempMax) {
    Cfg.tempMin = TEMP_MIN_C;
    Cfg.tempMax = TEMP_MAX_C;
  }
  for (uint8_t k = 0; k < 3; k++) {
    if (Cfg.quickTemp[k] > TEMP_MAX_C)
      Cfg.quickTemp[k] = TEMP_MAX_C;
  }

  // 手柄参数表越界保护
  if (Cfg.pidFollowTip > 1)
    Cfg.pidFollowTip = 1;
  for (uint8_t i = 0; i < TIP_TYPE_COUNT; i++) {
    if (Cfg.tipPowerPct[i] > 100)
      Cfg.tipPowerPct[i] = 0;
    if (Cfg.tipTempMax[i] < 0 || Cfg.tipTempMax[i] > TEMP_MAX_C)
      Cfg.tipTempMax[i] = 0;
    for (uint8_t k = 0; k < 3; k++) {
      float v = Cfg.tipPid[i][k];
      if (!(v >= 0.0f && v <= 200.0f)) // 同时拦截 NaN
        Cfg.tipPid[i][k] = (k == 0) ? 25.0f : ((k == 1) ? 0.3f : 15.0f);
    }
  }
  return true;
}

void Settings_Save() {
  Cfg.crc = crc8((const uint8_t *)&Cfg, offsetof(Settings_t, crc));

  // 对齐到 4 字节块写
  size_t len = (sizeof(Cfg) + 3u) & ~3u;

  EFM_Unlock();
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  EFM_SectorErase(SETTINGS_FLASH_ADDR);
  EFM_SequenceProgram(SETTINGS_FLASH_ADDR, (uint32_t)len, &Cfg);
  __set_PRIMASK(primask);
  EFM_Lock();
}

void Settings_Reset(bool partial) {
  if (partial)
    fillPartialDefaults();
  else
    fillDefaults();
  Settings_Save();
}
