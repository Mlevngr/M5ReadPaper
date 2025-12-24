#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include <stdint.h>
#include "ui_canvas_utils.h"

// 触摸区域枚举 - 6列×10行网格系统 (540×960, 每格90×96px)
enum class TouchZone {
    // 传统三分区（向后兼容）
    LEFT_THIRD,    // 左侧1/3
    MIDDLE_THIRD,  // 中间1/3
    RIGHT_THIRD,   // 右侧1/3
    FAKE_CURRENT,  // FAKE to trigger current page
    FAKE_JUMP,  // FAKE to trigger jump page
    
    // 6×10网格区域 (ROW_COL格式)
    ONE_ONE, ONE_TWO, ONE_THREE, ONE_FOUR, ONE_FIVE, ONE_SIX,
    TWO_ONE, TWO_TWO, TWO_THREE, TWO_FOUR, TWO_FIVE, TWO_SIX,
    THREE_ONE, THREE_TWO, THREE_THREE, THREE_FOUR, THREE_FIVE, THREE_SIX,
    FOUR_ONE, FOUR_TWO, FOUR_THREE, FOUR_FOUR, FOUR_FIVE, FOUR_SIX,
    FIVE_ONE, FIVE_TWO, FIVE_THREE, FIVE_FOUR, FIVE_FIVE, FIVE_SIX,
    SIX_ONE, SIX_TWO, SIX_THREE, SIX_FOUR, SIX_FIVE, SIX_SIX,
    SEVEN_ONE, SEVEN_TWO, SEVEN_THREE, SEVEN_FOUR, SEVEN_FIVE, SEVEN_SIX,
    EIGHT_ONE, EIGHT_TWO, EIGHT_THREE, EIGHT_FOUR, EIGHT_FIVE, EIGHT_SIX,
    NINE_ONE, NINE_TWO, NINE_THREE, NINE_FOUR, NINE_FIVE, NINE_SIX,
    TEN_ONE, TEN_TWO, TEN_THREE, TEN_FOUR, TEN_FIVE, TEN_SIX,
    
    UNKNOWN  // 未知区域
};

// 翻页结果
struct PageTurnResult {
    bool success;
    bool page_changed;
    const char* message;
};

// 触摸区域判断
TouchZone getTouchZoneGrid(int16_t touch_x, int16_t touch_y);   // 新6×10网格方式

// 处理阅读状态下的触摸翻页
PageTurnResult handleReadingTouch(TouchZone zone);

// 菜单触摸处理结果
// Besides the first several items, only using 'message' to judge the type , to simplify
struct MenuTouchResult {
    bool success;
    bool button_pressed;  // 是否按下了圆形按钮
    bool button_pwr_pressed;  // 是否按下了圆形按钮
    bool panel_clicked;   // 是否点击了菜单面板（非按钮区域）
    bool outside_clicked; // 是否点击了菜单以外的区域
    const char* message;
};

// 处理菜单状态下的触摸（使用 TouchZone 枚举）
MenuTouchResult handleMenuTouch(TouchZone zone);

// 处理主菜单状态下的触摸（使用 TouchZone 枚举）
MenuTouchResult handleMainMenuTouch(TouchZone zone);

#endif // UI_CONTROL_H
