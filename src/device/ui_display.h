#pragma once
#include <M5Unified.h>
#include "readpaper.h"
#include "../text/bin_font_print.h"

// 封装显示函数
void display_print(const char* text, float text_size = SYSFONTSIZE, uint16_t color = TFT_BLACK, uint8_t alignment = TL_DATUM, 
                     int16_t margin_top = 30, int16_t margin_bottom = 30, 
                     int16_t margin_left = 20, int16_t margin_right = 20,
                     uint16_t bgcolor = WHITE, bool fastmode = true, bool dark=false);

//Debug, just one warpper for display
void initDisplay();
void fontLoad();
// Wrapper to set display rotation while handling EPD power-save around the call.
// Ensures M5.Display.powerSaveOff() is called before setRotation and
// M5.Display.powerSaveOn() is called after to avoid partial-update issues.
void display_set_rotation(int rotation);