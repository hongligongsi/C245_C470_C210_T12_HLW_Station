/**
 * @file menu.h
 * @brief 树状设置菜单: 浏览/编辑/重置, 持久化到 Flash
 *
 * 导航(用户规格):
 *   旋转   = 上下移动选中 / 编辑时增减值
 *   短按   = 进入子菜单 / 进入编辑 / 确认 / 切换开关 / 执行动作
 *   长按   = 返回上级 / 退出菜单 (主页时长按=进入菜单)
 *
 * 菜单树:
 *   1.基本设置  声音/亮度/最低温/最高温/步进
 *   2.工具设置
 * 待机温/待机时间/休眠时间/快捷温1-3/一键升温/P/I/D/控制带/功率/滚珠开关
 *   3.主题风格  风格(标准/曲线)/主题(深/浅)
 *   4.系统重置  重置配置/系统重置
 *   5.关于本机  (INFO)
 */
#pragma once
#include <Arduino.h>

enum NodeType : uint8_t {
  NT_SUBMENU = 0,
  NT_BOOL,
  NT_INT,
  NT_FLOAT,
  NT_SELECT,
  NT_INFO,
  NT_ACTION, // 动作项(重置/一键升温)
};

struct MenuItem;

// 动作回调: on 为开关目标态(BOOST 用), 重置类忽略该参数
typedef void (*MenuActionFn)(bool on);
// 开关回读(BOOST 显示当前态 + 计算翻转基准), 无则返回 false
typedef bool (*MenuStateFn)();

struct MenuItem {
  const char *label;
  NodeType type;

  void *ptr;     // BOOL/INT/FLOAT/SELECT: 指向 Cfg 字段
  uint8_t psize; // INT/SELECT 指针宽度: 1 或 2 (字节)
  int32_t imin, imax, istep;
  float fmin, fmax, fstep;
  const char *const *opts; // SELECT 选项文本
  uint8_t optCount;
  const char *suffix; // 数值单位 "C"/"%"/"M"

  MenuActionFn action; // NT_ACTION
  MenuStateFn stateFn; // NT_ACTION 回读

  const MenuItem *children;
  uint8_t childCount;
};

class Menu {
public:
  void enter(); // 主页长按进入菜单
  void exit();  // 退出回主页
  bool active() const { return _active; }
  void invalidate() { _needsClear = true; } // 校准页覆盖后强制重画菜单

  void move(int16_t delta); // 旋转: 浏览移动 / 编辑增减
  void select();            // 短按
  void back();              // 长按: 返回/退出

  // 短按选中 CALIB 列表项时取出要校准的项(取出后内部清空), 用于启动全屏校准页
  const MenuItem *takeCalibItem() {
    const MenuItem *it = _calibPick;
    _calibPick = nullptr;
    return it;
  }

  void render(); // 由 UI 在菜单激活时调用

private:
  struct Level {
    const MenuItem *items;
    uint8_t count;
    int8_t sel;
    int8_t top;
    const char *title;
  };

  Level _stack[4];
  uint8_t _depth = 0;
  bool _active = false;
  bool _editing = false;
  bool _needsClear = true;
  const MenuItem *_editItem = nullptr;
  const MenuItem *_calibPick = nullptr; // 待启动全屏校准页的项
  int32_t _editI = 0;
  float _editF = 0;

  void pushLevel(const MenuItem *items, uint8_t count, const char *title);
  void popLevel();
  int8_t visibleRows() const;
  void ensureVisible();
  void commitEdit();
  void readItem(const MenuItem *it, int32_t &iv, float &fv);

  // 行渲染缓存(仅内容变化的行才重绘, 降低软 SPI 流量)
  struct RowCache {
    char label[14];
    char val[10];
    uint8_t flags; // bit0=选中 bit1=编辑中
  };
  RowCache _rc[16];
  char _titleCache[14];
  bool _titleValid = false;
};

extern Menu MenuCtl;
