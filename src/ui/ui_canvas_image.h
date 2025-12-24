#pragma once
#include <M5Unified.h>

// 将图片推送到Canvas指定位置，不立即刷新
// canvas: 目标画布，如果为nullptr则使用全局g_canvas
// x,y: 当使用自定义canvas时为附加偏移，否则为绝对位置
void ui_push_image_to_canvas(const char* img_path, int16_t x, int16_t y, M5Canvas* canvas = nullptr, bool preClean=false);

// 直接在display上推送图片，计算真实可见区域，实现最快显示
void ui_push_image_to_display_direct(const char* img_path, int16_t x, int16_t y, bool preClean = false);
