/**
 * @file tc_table.h
 * @brief K 型热电偶 EMF ↔ 温度 查表换算（NIST ITS-90 分度表）
 *
 * 对应逆向报告《固件逆向_核心算法还原报告》：
 *   - 0x63EA  温度查表（原厂通过 16 位码表把 ADC 通道值换成温度）
 *   - 0x2209C 温度换算（按传感器类型走不同系数）
 *   - 0x21E48 归一化（热电偶 / 冷端 分别乘固定标定系数）
 *
 * 原厂用"查表"而不是线性系数，是因为 K 型热电偶的塞贝克系数随温度变化
 * （0°C 附近约 39.7µV/°C，400°C 附近约 42.3µV/°C）。本模块用同一思路：
 * 10°C 步进的标准分度表 + 线性插值，替代早期固件的固定 41µV/°C 近似。
 * 固定 41µV/°C 的拟合残差在全段约 ±3.5°C（500°C 处 -3.5°C，250°C 处 +2.4°C），
 * 查表后只剩分段线性插值误差（表节点为 NIST 标准值，<0.1°C）。
 */
#pragma once
#include <stdint.h>

namespace TcTable {

// 温度(°C) → 热端电动势(µV)，超出表区间按末段斜率线性外推
float tempToEmf(float c);

// 热端电动势(µV) → 温度(°C)，0..500°C 查表插值，超出按末段斜率外推
float emfToTemp(float uv);

// 冷端补偿换算：测得电动势(µV，已扣除运放零位) + 冷端温度(°C) → 热端温度(°C)
// 原理：E_measured = E(T_hot) - E(T_cold)  =>  E(T_hot) = E_measured + E(T_cold)
float compensateAndConvert(float uvMeasured, float coldJunctionC);

} // namespace TcTable
