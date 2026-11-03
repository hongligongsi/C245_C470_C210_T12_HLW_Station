/**
 * @file main.cpp
 * @brief T12 / HLW245 / HLW210 / C470 通用焊台固件
 *
 * 硬件: HC32F460JETA-LQFP48 (依据 SCH_T12_HLW470-245-210_V1.7-高压版)
 *
 * 操作(用户规格):
 *   主页  旋转=调温(步进可设, 同时开机/唤醒)
 *         短按=循环 3 个快捷温度
 *         长按=进入菜单
 *   菜单  旋转=上下滚动 / 编辑时改值
 *         短按=进入/选择/确认
 *         长按=返回/退出
 *   休眠  长按=解除休眠
 *
 * 自动状态:
 *   放回支架 2s        休眠停热
 *   待机时间到期(可选) 保持待机温度
 *   休眠时间到期       停热
 *   HLW 换芯开关       立即停热
 *
 * 保护: 欠压 17V / 过压 50.5V / 超温 480°C / 过流 6A / 无烙铁头检测
 */
#include "about.h"
#include "board.h"
#include "buzzer.h"
#include "calib.h"
#include "encoder.h"
#include "menu.h"
#include "settings.h"
#include "station.h"
#include "ui.h"
#include <Arduino.h>

static uint32_t s_lastTickMs = 0;
static uint8_t s_blinkCnt = 0; // 背光软件 PWM 计数(1ms, 0..99)
static FaultCode s_lastFault = FAULT_NONE;
static bool s_lastTip = true;

// 蜂鸣器受 Cfg.soundOn 门控
static void beep(uint16_t ms) {
  if (Cfg.soundOn)
    Buzz.beep(ms);
}

// 故障报警节奏(对应原厂 TIMERA6 故障报警定时器 0x220D2: 故障期间持续报警):
// 超温/传感器故障三连短鸣, 其余双短鸣, 每 2s 一轮; 故障消失立即静音。
static uint32_t s_alarmNextMs = 0;
static uint8_t s_alarmBeeps = 0;

static void alarmTick(uint32_t now) {
  FaultCode f = Stn.fault();
  if (f == FAULT_NONE) {
    s_alarmBeeps = 0;
    s_alarmNextMs = 0;
    return;
  }
  if (s_alarmNextMs == 0) { // 故障刚出现: 让位于入口长鸣, 1.5s 后开始节奏
    s_alarmNextMs = now + 1500;
    return;
  }
  if ((int32_t)(now - s_alarmNextMs) < 0)
    return;
  uint8_t total = (f == FAULT_OVERTEMP || f == FAULT_TIPSENSOR) ? 3 : 2;
  beep((f == FAULT_OVERTEMP) ? 120 : 60);
  if (++s_alarmBeeps >= total) {
    s_alarmBeeps = 0;
    s_alarmNextMs = now + 2000;
  } else {
    s_alarmNextMs = now + 220;
  }
}

void setup() {
  // 先加载设置, Station/UI 初始化时读取 Cfg
  Settings_Load();

  Enc.begin();
  Buzz.begin();
  Stn.begin();
  Screen.begin();

  beep(25);
  s_lastTip = Stn.tipPresent();
  s_lastFault = Stn.fault();
}

void loop() {
  uint32_t now = millis();

  // 1ms 基准: 编码器扫描 + 背光软件 PWM(100Hz)
  if (now != s_lastTickMs) {
    s_lastTickMs = now;
    Enc.tick();

    s_blinkCnt++;
    if (s_blinkCnt >= 100)
      s_blinkCnt = 0;
    digitalWrite(PIN_TFT_LED, s_blinkCnt < Cfg.brightness ? HIGH : LOW);
  }

  // 蜂鸣器翻转 (尽量频繁)
  Buzz.tickMicros();

  // ---- 编码器旋转 ----
  int16_t delta = Enc.takeDelta();
  if (delta != 0) {
    if (Calib.active()) {
      Calib.move(delta);
      beep(3);
    } else if (About.active()) {
      // 信息页只读, 旋转无操作
    } else if (MenuCtl.active()) {
      MenuCtl.move(delta);
      beep(3);
    } else if (Stn.mode() != MODE_FAULT) {
      int16_t old = Stn.setTemp();
      Stn.adjustSetTemp(delta);
      if (Stn.setTemp() != old)
        beep(6);
    }
  }

  // ---- 编码器按键 ----
  EncEvent ev = Enc.takeEvent();
  if (ev == ENC_EVT_CLICK) {
    if (Calib.active()) {
      // 校准页短按: 保存并退回校准项列表
      Calib.select();
      MenuCtl.invalidate();
      beep(20);
    } else if (About.active()) {
      // 信息页短按: 返回菜单
      About.select();
      MenuCtl.invalidate();
      beep(20);
    } else if (MenuCtl.active()) {
      MenuCtl.select();
      const MenuItem *ci = MenuCtl.takeCalibItem();
      if (ci)
        Calib.enter(ci); // 校准项 -> 全屏可视化校准页
      if (MenuCtl.takeInfoPick())
        About.enter(); // 关于本机 -> 全屏信息页
      beep(15);
    } else if (Stn.mode() == MODE_FAULT) {
      Stn.clearFault();
      beep(30);
    } else {
      // 主页短按: 循环快捷温度
      int16_t t = Stn.cycleQuickTemp();
      Screen.showQuickTempPrompt(t);
      beep(15);
    }
  } else if (ev == ENC_EVT_HOLD) {
    if (Calib.active()) {
      // 校准页长按: 放弃修改退回列表
      Calib.back();
      MenuCtl.invalidate();
      beep(20);
    } else if (About.active()) {
      // 信息页长按: 返回菜单
      About.back();
      MenuCtl.invalidate();
      beep(20);
    } else if (MenuCtl.active()) {
      MenuCtl.back();
      beep(20);
    } else if (Stn.mode() == MODE_SLEEP) {
      Stn.wakeFromSleep();
      beep(40);
    } else if (Stn.mode() != MODE_FAULT) {
      // 主页长按: 进入菜单
      MenuCtl.enter();
      beep(40);
    }
  }

  // ---- 事件提示音 ----
  if (Stn.fault() != s_lastFault) {
    if (Stn.fault() != FAULT_NONE)
      beep(120); // 进入故障: 长鸣
    s_lastFault = Stn.fault();
  }
  if (Stn.tipPresent() != s_lastTip) {
    beep(Stn.tipPresent() ? 20 : 100);
    s_lastTip = Stn.tipPresent();
  }

  // ---- 业务与界面 ----
  Stn.task();
  alarmTick(now);
  Screen.update();
}
