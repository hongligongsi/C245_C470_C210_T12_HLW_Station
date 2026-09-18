/**
 * @file settings.h
 * @brief 设置持久化: 片内 Flash 模拟 EEPROM
 *        存于末扇区 0x0007F800 (2KB), 远离代码区
 */
#pragma once
#include <Arduino.h>

#define SETTINGS_MAGIC 0x5A4Cu
#define SETTINGS_VERSION 2u // v2: 新增 tempCal(350° 单点增益)

// 系统风格
#define STYLE_STANDARD 0
#define STYLE_CURVE 1
// 系统主题
#define THEME_DARK 0
#define THEME_LIGHT 1

struct Settings_t {
  uint16_t magic;
  uint16_t version;

  // --- 基本设置 ---
  uint8_t soundOn;    // 1=蜂鸣器开
  uint8_t brightness; // 1..100 背光占空比
  int16_t tempMin;    // 最低温度
  int16_t tempMax;    // 最高温度
  uint8_t tempStep;   // 旋转步进
  uint8_t _pad0;

  // --- 工具设置 ---
  int16_t standbyTemp;    // 待机温度
  uint8_t standbyTimeMin; // 待机时间(震动开关), 0=禁用
  uint8_t sleepTimeMin;   // 休眠时间(min), 到期停热
  int16_t quickTemp[3];   // 快捷温度
  uint8_t boostOn;        // 一键升温开关
  uint8_t vibEnable;      // 滚珠/震动开关使能(待机时间=0 时无意义)
  float pidP;             // 比例
  float pidI;             // 积分
  float pidD;             // 微分
  float pidBand;          // PID控制带: |err|>此值全速加热且不积分
  float pidIntegralMax;   // 积分限幅
  uint8_t powerLimitPct;  // 功率上限 %
  uint8_t _pad2[3];

  // --- 主题风格 ---
  uint8_t style; // 0=标准 1=曲线
  uint8_t theme; // 0=深色 1=浅色
  uint8_t _pad3[2];
  // --- 校准 ---
  float vbusK;     // 电压分压下电阻等效(覆盖 VDIV_RBOT_K)
  float cjcOffset; // 冷结偏移°C(覆盖 CJC_OFFSET_C)
  float tempCal;   // 350° 单点温度增益%, 100.0=不修正(作用于热电偶温升)

  // --- CRC ---
  uint8_t crc; // 覆盖 magic ~ crc 前一字节
  uint8_t _pad4[3];
};

// 全局配置实例(运行时读写)
extern Settings_t Cfg;

// 启动加载, 返回是否命中有效存储
bool Settings_Load();
// 保存到 Flash(擦+写)
void Settings_Save();
// 复位: partial=true 只重置基本+工具(对应"重置配置"), false 全复位
void Settings_Reset(bool partial);
