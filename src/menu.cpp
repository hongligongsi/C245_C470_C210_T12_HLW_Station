/**
 * @file menu.cpp
 * @brief 树状设置菜单实现: 浏览/编辑/重置, 改值即写入 Flash
 */
#include "menu.h"
#include "settings.h"
#include "station.h"
#include "tft.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

Menu MenuCtl;

// 布局
#define MENU_SCALE ((LCD_W >= 120) ? 2 : 1)
#define M_CH (7 * MENU_SCALE) // 字高
#define M_PITCH (6 * MENU_SCALE)
#define ROW_H (10 * MENU_SCALE)
#define TITLE_H (10 * MENU_SCALE)

// ---------------------------------------------------------------------------
// SELECT 选项
// ---------------------------------------------------------------------------
static const char *const STYLE_OPTS[2] = {"STD", "CURVE"};
static const char *const THEME_OPTS[2] = {"DARK", "LIGHT"};

// ---------------------------------------------------------------------------
// ACTION 回调
// ---------------------------------------------------------------------------
static void actResetCfg(bool /*on*/) {
  Settings_Reset(true);
  Stn.syncConfig();
}
static void actResetAll(bool /*on*/) {
  Settings_Reset(false);
  Stn.syncConfig();
}
static void actBoost(bool on) { Stn.setBoost(on); }
static bool stateBoost() { return Stn.boostActive(); }

// ---------------------------------------------------------------------------
// 菜单树
// ---------------------------------------------------------------------------
// 1.基本设置
static const MenuItem s_basic[] = {
    {"SOUND", NT_BOOL, &Cfg.soundOn, 1, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"BRIGHT", NT_INT, &Cfg.brightness, 1, 1, 100, 1, 0.0f, 0.0f, 0.0f, nullptr,
     0, "%", nullptr, nullptr, nullptr, 0},
    {"MIN TMP", NT_INT, &Cfg.tempMin, 2, 50, 400, 5, 0.0f, 0.0f, 0.0f, nullptr,
     0, "C", nullptr, nullptr, nullptr, 0},
    {"MAX TMP", NT_INT, &Cfg.tempMax, 2, 200, 500, 5, 0.0f, 0.0f, 0.0f, nullptr,
     0, "C", nullptr, nullptr, nullptr, 0},
    {"STEP", NT_INT, &Cfg.tempStep, 1, 1, 50, 1, 0.0f, 0.0f, 0.0f, nullptr, 0,
     "C", nullptr, nullptr, nullptr, 0},
};

// 2.工具设置
static const MenuItem s_tools[] = {
    {"STBY T", NT_INT, &Cfg.standbyTemp, 2, 50, 300, 5, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "C", nullptr, nullptr, nullptr, 0},
    {"STBY M", NT_INT, &Cfg.standbyTimeMin, 1, 0, 30, 1, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "M", nullptr, nullptr, nullptr, 0},
    {"SLEEP M", NT_INT, &Cfg.sleepTimeMin, 1, 0, 120, 1, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "M", nullptr, nullptr, nullptr, 0},
    {"Q TMP1", NT_INT, &Cfg.quickTemp[0], 2, 50, 500, 5, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "C", nullptr, nullptr, nullptr, 0},
    {"Q TMP2", NT_INT, &Cfg.quickTemp[1], 2, 50, 500, 5, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "C", nullptr, nullptr, nullptr, 0},
    {"Q TMP3", NT_INT, &Cfg.quickTemp[2], 2, 50, 500, 5, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "C", nullptr, nullptr, nullptr, 0},
    {"BOOST", NT_ACTION, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, actBoost, stateBoost, nullptr, 0},
    {"P", NT_FLOAT, &Cfg.pidP, 4, 0, 0, 0, 1.0f, 100.0f, 0.5f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"I", NT_FLOAT, &Cfg.pidI, 4, 0, 0, 0, 0.0f, 5.0f, 0.1f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"D", NT_FLOAT, &Cfg.pidD, 4, 0, 0, 0, 0.0f, 100.0f, 0.5f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"BAND", NT_FLOAT, &Cfg.pidBand, 4, 0, 0, 0, 1.0f, 100.0f, 1.0f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"POWER", NT_INT, &Cfg.powerLimitPct, 1, 10, 100, 5, 0.0f, 0.0f, 0.0f,
     nullptr, 0, "%", nullptr, nullptr, nullptr, 0},
    // 滚珠开关: 待机时间=0 时此项无意义; 需硬件引接 PIN_VIBRATION
    {"VIB SW", NT_BOOL, &Cfg.vibEnable, 1, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr,
     0, nullptr, nullptr, nullptr, nullptr, 0},
};

// 3.主题风格
static const MenuItem s_theme[] = {
    {"STYLE", NT_SELECT, &Cfg.style, 1, 0, 1, 1, 0, 0, 0, STYLE_OPTS, 2,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"THEME", NT_SELECT, &Cfg.theme, 1, 0, 1, 1, 0, 0, 0, THEME_OPTS, 2,
     nullptr, nullptr, nullptr, nullptr, 0},
};

// 4.校准 (全屏可视化校准页: 实时 V/I/P/T + 曲线; 改动立即生效)
static const MenuItem s_calib[] = {
    {"VBUS K", NT_FLOAT, &Cfg.vbusK, 4, 0, 0, 0, 1.0f, 10.0f, 0.1f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
    {"CJC", NT_FLOAT, &Cfg.cjcOffset, 4, 0, 0, 0, -30.0f, 30.0f, 0.5f, nullptr,
     0, "C", nullptr, nullptr, nullptr, 0},
    {"T CAL", NT_FLOAT, &Cfg.tempCal, 4, 0, 0, 0, 80.0f, 120.0f, 0.1f, nullptr,
     0, "%", nullptr, nullptr, nullptr, 0},
};

// 5.系统重置
static const MenuItem s_reset[] = {
    {"CFG RST", NT_ACTION, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, actResetCfg, nullptr, nullptr, 0},
    {"SYS RST", NT_ACTION, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, actResetAll, nullptr, nullptr, 0},
};

// 根菜单
static const MenuItem s_root[] = {
    {"BASIC", NT_SUBMENU, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, s_basic, 5},
    {"TOOLS", NT_SUBMENU, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, s_tools, 13},
    {"THEME", NT_SUBMENU, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, s_theme, 2},
    {"CALIB", NT_SUBMENU, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, s_calib, 3},
    {"RESET", NT_SUBMENU, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, s_reset, 2},
    {"ABOUT", NT_INFO, nullptr, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, 0,
     nullptr, nullptr, nullptr, nullptr, 0},
};

// ---------------------------------------------------------------------------
// 整型读写(支持 uint8 / int16 两种字段宽度)
// ---------------------------------------------------------------------------
static int32_t readInt(const MenuItem *it) {
  if (it->psize == 2) {
    int16_t h;
    memcpy(&h, it->ptr, 2);
    return h;
  }
  uint8_t b;
  memcpy(&b, it->ptr, 1);
  return b;
}

static void writeInt(const MenuItem *it, int32_t v) {
  if (it->psize == 2) {
    int16_t h = (int16_t)v;
    memcpy(it->ptr, &h, 2);
  } else {
    uint8_t b = (uint8_t)v;
    memcpy(it->ptr, &b, 1);
  }
}

// ---------------------------------------------------------------------------
// 导航
// ---------------------------------------------------------------------------
void Menu::enter() {
  _depth = 1;
  _stack[0].items = s_root;
  _stack[0].count = 6;
  _stack[0].sel = 0;
  _stack[0].top = 0;
  _stack[0].title = "MENU";
  _editing = false;
  _editItem = nullptr;
  _active = true;
  _needsClear = true;
  _titleValid = false;
}

void Menu::exit() {
  _active = false;
  _editing = false;
  Screen.invalidate();
}

void Menu::pushLevel(const MenuItem *items, uint8_t count, const char *title) {
  if (_depth >= 4)
    return;
  Level &l = _stack[_depth++];
  l.items = items;
  l.count = count;
  l.sel = 0;
  l.top = 0;
  l.title = title;
  _needsClear = true;
  _titleValid = false;
}

void Menu::popLevel() {
  if (_depth > 1) {
    _depth--;
    _needsClear = true;
    _titleValid = false;
  } else {
    exit();
  }
}

int8_t Menu::visibleRows() const { return (int8_t)((LCD_H - TITLE_H) / ROW_H); }

void Menu::ensureVisible() {
  Level &l = _stack[_depth - 1];
  int8_t rows = visibleRows();
  if (l.sel < l.top)
    l.top = l.sel;
  if (l.sel >= l.top + rows)
    l.top = l.sel - rows + 1;
  if (l.top < 0)
    l.top = 0;
  if (l.top > l.count - rows && l.count > rows)
    l.top = l.count - rows;
}

void Menu::move(int16_t delta) {
  if (!_active || delta == 0)
    return;
  Level &l = _stack[_depth - 1];

  if (_editing) {
    if (_editItem->type == NT_FLOAT) {
      _editF += (float)delta * _editItem->fstep;
      if (_editF < _editItem->fmin)
        _editF = _editItem->fmin;
      if (_editF > _editItem->fmax)
        _editF = _editItem->fmax;
    } else if (_editItem->type == NT_SELECT) {
      int32_t v = _editI + delta;
      int32_t n = _editItem->optCount;
      while (v < 0)
        v += n;
      while (v >= n)
        v -= n;
      _editI = v;
    } else {
      _editI += (int32_t)delta * _editItem->istep;
      if (_editI < _editItem->imin)
        _editI = _editItem->imin;
      if (_editI > _editItem->imax)
        _editI = _editItem->imax;
    }
    return;
  }

  l.sel += delta;
  if (l.sel < 0)
    l.sel = 0;
  if (l.sel >= l.count)
    l.sel = l.count - 1;
  ensureVisible();
}

void Menu::commitEdit() {
  if (!_editItem)
    return;
  if (_editItem->type == NT_FLOAT) {
    if (_editF < _editItem->fmin)
      _editF = _editItem->fmin;
    if (_editF > _editItem->fmax)
      _editF = _editItem->fmax;
    *(float *)_editItem->ptr = _editF;
  } else {
    if (_editItem->type == NT_SELECT) {
      if (_editI < 0)
        _editI = 0;
      if (_editI >= _editItem->optCount)
        _editI = _editItem->optCount - 1;
    } else {
      if (_editI < _editItem->imin)
        _editI = _editItem->imin;
      if (_editI > _editItem->imax)
        _editI = _editItem->imax;
    }
    writeInt(_editItem, _editI);
  }
  Settings_Save();
  Stn.syncConfig();
}

void Menu::readItem(const MenuItem *it, int32_t &iv, float &fv) {
  iv = 0;
  fv = 0.0f;
  if (it->type == NT_FLOAT)
    fv = *(float *)it->ptr;
  else
    iv = readInt(it);
}

void Menu::select() {
  if (!_active)
    return;
  Level &l = _stack[_depth - 1];
  const MenuItem *it = &l.items[l.sel];

  if (_editing) {
    commitEdit();
    _editing = false;
    _editItem = nullptr;
    return;
  }

  // 校准列表项: 交给全屏可视化校准页(不进入内置数字编辑)
  if (it->type == NT_FLOAT && l.items == s_calib) {
    _calibPick = it;
    return;
  }

  switch (it->type) {
  case NT_SUBMENU:
    pushLevel(it->children, it->childCount, it->label);
    break;
  case NT_BOOL: {
    uint8_t *p = (uint8_t *)it->ptr;
    *p = !*p;
    Settings_Save();
  } break;
  case NT_INT:
  case NT_FLOAT:
  case NT_SELECT:
    _editing = true;
    _editItem = it;
    readItem(it, _editI, _editF);
    break;
  case NT_ACTION:
    if (it->action) {
      if (it->stateFn)
        it->action(!it->stateFn());
      else
        it->action(false); // 重置类: 自行 save
    }
    break;
  case NT_INFO:
  default:
    break; // 关于本机: 无操作
  }
}

void Menu::back() {
  if (!_active)
    return;
  if (_editing) {
    _editing = false; // 长按取消编辑, 不保存
    _editItem = nullptr;
    return;
  }
  popLevel();
}

// ---------------------------------------------------------------------------
// 渲染
// ---------------------------------------------------------------------------
static void buildValue(const MenuItem *it, char *out, size_t n) {
  out[0] = 0;
  switch (it->type) {
  case NT_SUBMENU:
    snprintf(out, n, ">");
    break;
  case NT_BOOL:
    snprintf(out, n, "%s", *(uint8_t *)it->ptr ? "ON" : "OFF");
    break;
  case NT_INT: {
    int32_t v = readInt(it);
    if (it->suffix)
      snprintf(out, n, "%d%s", (int)v, it->suffix);
    else
      snprintf(out, n, "%d", (int)v);
  } break;
  case NT_FLOAT:
    snprintf(out, n, "%.1f", *(float *)it->ptr);
    break;
  case NT_SELECT: {
    int32_t v = readInt(it);
    if (it->opts && v >= 0 && v < it->optCount)
      snprintf(out, n, "%s", it->opts[v]);
  } break;
  case NT_ACTION:
    if (it->stateFn)
      snprintf(out, n, "%s", it->stateFn() ? "ON" : "OFF");
    else
      snprintf(out, n, "RUN");
    break;
  case NT_INFO:
  default:
    snprintf(out, n, "-");
    break;
  }
}

void Menu::render() {
  if (!_active)
    return;

  if (_needsClear) {
    Lcd.clear(Pal.bg);
    for (uint8_t i = 0; i < 16; i++)
      _rc[i].flags = 0xFF;
    _titleValid = false;
    _needsClear = false;
  }

  Level &l = _stack[_depth - 1];

  // --- 标题栏(反色) ---
  if (!_titleValid || strncmp(_titleCache, l.title, sizeof(_titleCache)) != 0) {
    Lcd.fillRect(0, 0, LCD_W, TITLE_H, Pal.fg);
    int16_t ty = (TITLE_H - M_CH) / 2;
    Lcd.drawText(2, ty, l.title, Pal.bg, Pal.fg, MENU_SCALE);
    // 返回提示符(子菜单时)
    if (_depth > 1) {
      const char *bk = "<";
      int16_t w = Lcd.textWidth(bk, MENU_SCALE);
      Lcd.drawText(LCD_W - w - 2, ty, bk, Pal.bg, Pal.fg, MENU_SCALE);
    }
    Lcd.drawHLine(0, TITLE_H, LCD_W, Pal.grid);
    strncpy(_titleCache, l.title, sizeof(_titleCache) - 1);
    _titleCache[sizeof(_titleCache) - 1] = 0;
    _titleValid = true;
  }

  // --- 行 ---
  int8_t rows = visibleRows();
  int8_t n = l.count;
  if (n > rows)
    n = rows;

  for (int8_t r = 0; r < n; r++) {
    int8_t idx = l.top + r;
    if (idx >= l.count)
      break;
    const MenuItem *it = &l.items[idx];
    bool sel = (idx == l.sel);
    bool edit = _editing && _editItem == it;
    uint8_t flags = (sel ? 1 : 0) | (edit ? 2 : 0);

    char lab[16];
    snprintf(lab, sizeof(lab), "%s%s", edit ? "#" : "", it->label);
    char val[10];
    buildValue(it, val, sizeof(val));
    // 编辑中显示暂存值而非已存值
    if (edit) {
      if (it->type == NT_FLOAT)
        snprintf(val, sizeof(val), "%.1f", _editF);
      else if (it->type == NT_SELECT)
        snprintf(val, sizeof(val), "%s",
                 it->opts[_editI < it->optCount ? _editI : 0]);
      else {
        if (it->suffix)
          snprintf(val, sizeof(val), "%d%s", (int)_editI, it->suffix);
        else
          snprintf(val, sizeof(val), "%d", (int)_editI);
      }
    }

    RowCache &c = _rc[r];
    if (c.flags == flags && strcmp(c.label, lab) == 0 &&
        strcmp(c.val, val) == 0)
      continue; // 本行无变化

    int16_t y = TITLE_H + r * ROW_H;
    uint16_t rowBg = sel ? Pal.fg : Pal.bg;
    uint16_t labCol = sel ? Pal.bg : Pal.fg;
    uint16_t valCol = sel ? Pal.bg : (edit ? Pal.warn : Pal.info);

    Lcd.fillRect(0, y, LCD_W, ROW_H, rowBg);
    int16_t ty = y + (ROW_H - M_CH) / 2;
    Lcd.drawText(2, ty, lab, labCol, rowBg, MENU_SCALE);
    int16_t vw = Lcd.textWidth(val, MENU_SCALE);
    if (vw > LCD_W - 30)
      vw = LCD_W - 30; // 极窄屏保护
    Lcd.drawText(LCD_W - vw - 2, ty, val, valCol, rowBg, MENU_SCALE);

    strncpy(c.label, lab, sizeof(c.label) - 1);
    c.label[sizeof(c.label) - 1] = 0;
    strncpy(c.val, val, sizeof(c.val) - 1);
    c.val[sizeof(c.val) - 1] = 0;
    c.flags = flags;
  }
}
