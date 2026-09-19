/**
 * @file board.h
 * @brief 板级引脚与硬件参数定义
 *        依据 SCH_T12_HLW470-245-210_V1.7-高压版 原理图
 *
 * 主控: HC32F460JETA-LQFP48
 */
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// 引脚定义 (与原理图网络标号一一对应)
// ---------------------------------------------------------------------------
// EC11 旋转编码器 (33k/10k 上拉 + RC 滤波, 低电平有效)
#define PIN_ENC_B PA0 // EC11-B
#define PIN_ENC_A PA1 // EC11-A
#define PIN_ENC_C PA2 // EC11-C  按键

// HLW 手柄座 / 换芯 (U10, 33k 上拉, 低电平有效)
#define PIN_HLW_SW PA3  // HLW_SW  换芯/支架
#define PIN_HLW_SLE PA4 // HLW_SLE 休眠

// 模拟输入
#define PIN_GET_ID PA5      // GET_ID      手柄ID识别
#define PIN_ADC_CURRENT PA6 // ADC_CURRENT 加热电流
#define PIN_T12_AD PA7      // T12-AD      烙铁头温度(运放输出)
#define PIN_PWR_ADC PB0     // PWR_ADC     24-48V 输入电压

// T12 支架休眠信号 (GX12-5P pin4, 33k 上拉, 低电平有效)
#define PIN_SLEEP PA8

// 加热 PWM (TIMA1_CH2 硬件PWM, 高电平=加热, Q3+P-MOS XRS80P06G)
#define PIN_HEATER_PWM PA9

// SPI 屏 (软件 SPI; PA15=CLK PB4=SDA)
#define PIN_TFT_CS PA14
#define PIN_TFT_CLK PA15
#define PIN_TFT_SDA PB4
#define PIN_TFT_DC PB5 // TFT_A0
#define PIN_TFT_RST PB6
#define PIN_TFT_LED PB9 // 背光, 高电平点亮 (R19 10k->Q1 MMBT5551->TFT_LEDK)

// 蜂鸣器 (R22 1k->Q4 MMBT5551, R25 10k 基极下拉, 2.7kHz 无源蜂鸣器)
#define PIN_BEEP PB8

// ---------------------------------------------------------------------------
// 模拟通道硬件参数
// ---------------------------------------------------------------------------
// ADC: 12bit, VREF 3.3V
#define ADC_VREF_MV 3300.0f
#define ADC_FULL 4095.0f

// 热电偶运放 U1.2 (GS8552): 差分增益 G = R9/R10 = 1M/4.7k
// 可在 platformio.ini 用 -DTC_AMP_GAIN=xxx 覆盖以校准温度
#ifndef TC_AMP_GAIN
#define TC_AMP_GAIN (1000000.0f / 4700.0f) // 212.77
#endif
// K 型热电偶灵敏度 (约 41 uV/°C, 0-400°C 平均)
#define TC_SEEBEK_UV_PER_C 41.0f
// T12-AD 断线判定: R14(100k) 上拉, 烙铁头未接时运放输出接近满幅
#define TIP_RAW_OPEN 3750    // > 3.03V 认为无烙铁头
#define TIP_RAW_PRESENT 3550 // 回差
// 冷结补偿: 芯片内部温度相对手柄接口温度的偏移 (°C), 可在菜单校准
#ifndef CJC_OFFSET_C
#define CJC_OFFSET_C (-3.0f)
#endif

// 输入电压分压: R1=47k 上, R3=5k 电位器(下, 实物需标定)
// Vbus = Vadc * (Rtop+Rbot)/Rbot, 以下为默认值, 用实测电压在菜单中校准
#ifndef VDIV_RTOP_K
#define VDIV_RTOP_K 47.0f
#endif
#ifndef VDIV_RBOT_K
#define VDIV_RBOT_K 3.30f
#endif
#define VBUS_DIV_K ((VDIV_RTOP_K + VDIV_RBOT_K) / VDIV_RBOT_K)

// 电流采样: R30=5mΩ, U1.1 增益 G = R24/R34 = 200k/10k = 20
// V = I * 0.005 * 20 = I*0.1  =>  I(A) = Vadc / 0.1
#define CURRSENSE_GAIN 20.0f
#define CURRSENSE_R_MOHM 5.0f
#define CURRENT_V_PER_A (CURRSENSE_R_MOHM / 1000.0f * CURRSENSE_GAIN) // 0.1 V/A

// 保护阈值
#define VBUS_UV_FAULT 17.0f  // 欠压 V
#define VBUS_OV_FAULT 50.5f  // 过压 V (48V 电源上限留余量)
#define TEMP_FAULT_C 480.0f  // 超温保护
#define CURRENT_FAULT_A 6.0f // 过流保护

// PID 输出的"功率%"以该标称电压下的满功率为基准:
// 实际占空比按 (NOMINAL_VBUS/Vbus)^2 前馈补偿(P=U^2/R),
// 使 19V/24V/32V 等不同电源下的控温增益与实际功率保持一致。
// 24V 时补偿系数=1(与旧行为完全相同), 可用 -DNOMINAL_VBUS=xx 覆盖。
#ifndef NOMINAL_VBUS
#define NOMINAL_VBUS 24.0f
#endif
// 前馈系数上限: 异常低压时限制占空比放大倍数(欠压故障本身会停热)
#ifndef VFF_GAIN_MAX
#define VFF_GAIN_MAX 4.0f
#endif

// 温度通道合理性检测: 加热占空比持续 >SENSOR_NO_RISE_DUTY 达 SENSOR_NO_RISE_MS
// 而温度上升不足 SENSOR_NO_RISE_C -> 判温度信号异常(传感器短路/运放故障/MOS
// 不加热)
#define SENSOR_NO_RISE_DUTY 128          // >50% 占空比
#define SENSOR_NO_RISE_MS (8UL * 1000UL) // 持续 8s
#define SENSOR_NO_RISE_C 5.0f            // 温升不足 5C

// C210 发热芯极细, 满功率长时间加热易烧毁/过热:
// 识别为 C210 时输出强制钳位到该百分比(参考 KSGER 类固件, C210 限 60%)。
// C245/T12 不钳位, 仍受用户 POWER 菜单的全局功率上限约束。
#ifndef C210_POWER_LIMIT_PCT
#define C210_POWER_LIMIT_PCT 60.0f
#endif

// ---------------------------------------------------------------------------
// 控制参数
// ---------------------------------------------------------------------------
#define PWM_MAX 255
#define TEMP_MIN_C 100
#define TEMP_MAX_C 450
#define TEMP_DEFAULT_C 320
#define TEMP_BOOST_ADD_C 48 // 一键升温增量

#define HOLD_SLEEP_MS 2000 // 支架信号持续 -> 休眠
#define SW_TIPOFF_MS 300   // 换芯开关持续 -> 停热
#define BOOST_MAX_MS (60UL * 1000UL)
// 以下时间改为运行时可配(settings), 宏仅作编译期兜底默认
#define TEMP_SLEEP_C 150   // 支架休眠温度(休眠时停热, 此值仅参考)
#define TEMP_STANDBY_C 100 // 待机温度默认
#define STANDBY_MS                                                             \
  (5UL * 60UL * 1000UL) // [弃用] 旧默认, 运行时用 Cfg.standbyTimeMin
#define POWEROFF_MS                                                            \
  (15UL * 60UL * 1000UL) // [弃用] 旧默认, 运行时用 Cfg.sleepTimeMin

#define BEEP_FREQ_HZ 2700

// ---------------------------------------------------------------------------
// 片内 Flash 模拟 EEPROM (存设置)
// HC32F460JETA=512KB(xE), 末 2KB 扇区 0x0007F800 远离代码区(固件<64KB)
// ---------------------------------------------------------------------------
#define SETTINGS_FLASH_ADDR 0x0007F800u
#define FLASH_SECTOR_SIZE 2048u

// 震动/滚珠开关引脚: 本 V1.7 板未引出, 不定义则相关逻辑跳过
// 若有该硬件, 在 platformio.ini 加 -DPIN_VIBRATION=PAx
#ifndef PIN_VIBRATION
// #define PIN_VIBRATION PAx
#endif

// ---------------------------------------------------------------------------
// GET_ID(PA5) 手柄型号识别 (GX16-5 端子3=ID, 板内上拉至 3.3V, ADC 12bit):
//   C210: ID(蓝线) 直接接地        -> raw ≈ 0
//   T12 : ID(绿线) 经 10K 电阻接地 -> raw 居中(上拉 10K 时≈2048, 上拉越大越低)
//   C245: ID(蓝线) 悬空            -> raw ≈ 满量程
// 两档边界(实测后可在 platformio.ini 用 -D 覆盖):
//   raw <  TIPID_TH_C210   -> C210
//   raw <  TIPID_TH_C245   -> T12
//   raw >= TIPID_TH_C245   -> C245
// ---------------------------------------------------------------------------
#ifndef TIPID_TH_C210
#define TIPID_TH_C210 400
#endif
#ifndef TIPID_TH_C245
#define TIPID_TH_C245 3000
#endif
