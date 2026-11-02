/**
 * @file tc_table.cpp
 * @brief K 型热电偶查表换算实现（NIST ITS-90 标准分度表，0~500°C / 10°C 步进）
 */
#include "tc_table.h"

namespace {

// K 型热电偶标准分度表：温度 0..500°C，步进 10°C，单位 µV
// 例：100°C=4096µV, 200°C=8138µV, 300°C=12209µV, 400°C=16397µV, 500°C=20644µV
// (NIST ITS-90；全段斜率 39.7~42.6µV/°C，用固定系数近似会有 ±3.5°C 残差)
const float K_EMF_UV[] = {
    0.0f,     397.0f,   798.0f,   1203.0f,  1612.0f,  2023.0f,  2436.0f,
    2851.0f,  3267.0f,  3682.0f,  4096.0f,  4509.0f,  4920.0f,  5328.0f,
    5735.0f,  6138.0f,  6540.0f,  6941.0f,  7340.0f,  7739.0f,  8138.0f,
    8539.0f,  8940.0f,  9343.0f,  9747.0f,  10153.0f, 10561.0f, 10971.0f,
    11382.0f, 11795.0f, 12209.0f, 12624.0f, 13040.0f, 13457.0f, 13874.0f,
    14293.0f, 14712.0f, 15132.0f, 15552.0f, 15974.0f, 16397.0f, 16820.0f,
    17243.0f, 17667.0f, 18091.0f, 18516.0f, 18941.0f, 19366.0f, 19792.0f,
    20218.0f, 20644.0f,
};

const int K_N = (int)(sizeof(K_EMF_UV) / sizeof(K_EMF_UV[0]));
const float K_STEP_C = 10.0f;
const float K_TOP_C = (float)(K_N - 1) * K_STEP_C; // 500°C
// 末段斜率(µV/°C)，用于 500°C 以上外推
const float K_TOP_SLOPE = (K_EMF_UV[K_N - 1] - K_EMF_UV[K_N - 2]) / K_STEP_C;

} // namespace

namespace TcTable {

float tempToEmf(float c) {
  if (c <= 0.0f)
    return 0.0f;
  if (c >= K_TOP_C)
    return K_EMF_UV[K_N - 1] + (c - K_TOP_C) * K_TOP_SLOPE;
  float pos = c / K_STEP_C;
  int i = (int)pos; // 0..K_N-2
  float f = pos - (float)i;
  return K_EMF_UV[i] + (K_EMF_UV[i + 1] - K_EMF_UV[i]) * f;
}

float emfToTemp(float uv) {
  if (uv <= 0.0f)
    return 0.0f;
  if (uv >= K_EMF_UV[K_N - 1])
    return K_TOP_C + (uv - K_EMF_UV[K_N - 1]) / K_TOP_SLOPE;

  // 二分定位区间后线性插值（51 项，最多 6 次比较）
  int lo = 0;
  int hi = K_N - 1;
  while (hi - lo > 1) {
    int mid = (lo + hi) >> 1;
    if (K_EMF_UV[mid] <= uv)
      lo = mid;
    else
      hi = mid;
  }
  float span = K_EMF_UV[hi] - K_EMF_UV[lo];
  if (span <= 0.0f)
    return (float)lo * K_STEP_C;
  return (float)lo * K_STEP_C + K_STEP_C * (uv - K_EMF_UV[lo]) / span;
}

float compensateAndConvert(float uvMeasured, float coldJunctionC) {
  return emfToTemp(uvMeasured + tempToEmf(coldJunctionC));
}

} // namespace TcTable
