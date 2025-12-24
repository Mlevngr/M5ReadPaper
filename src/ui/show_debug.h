#pragma once
#include <M5Unified.h>

// Draw the debug UI into provided canvas (upper half: three vertically stacked buttons centered).
// Returns true if drawing succeeded.
bool show_debug(M5Canvas *canvas=nullptr, bool refresh=false);

// Test whether touch at (tx,ty) hits one of the debug buttons.
// Index: 0 => "SPIFFS测试", 1 => "SD测试", 2 => "USBMSC测试"
bool debug_button_hit(int index, int16_t tx, int16_t ty);

// Utility to get button bounds (cx, cy, w, h) for index
void debug_button_bounds(int index, int16_t &cx, int16_t &cy, int16_t &w, int16_t &h);
