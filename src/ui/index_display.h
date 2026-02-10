#pragma once

#include <M5Unified.h>

// 显示当前书籍的 tag 列表（左侧 360x800 区域，10 行）
// 如果 canvas 为 nullptr，则使用全局 g_canvas
// paging : 0 - 全屏刷， 2- 刷列表部分（用于书签目录跳转）
void show_tag_ui(M5Canvas *canvas = nullptr, int8_t paging = 0);
