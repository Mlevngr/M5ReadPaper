#include "font_color_mapper.h"
#include "readpaper.h"
#include <M5Unified.h>

/**
 * @brief 将像素值映射为RGB565颜色
 */
uint16_t FontColorMapper::map_pixel_to_color(uint8_t pixel_value, uint8_t font_version,
                                            bool dark_mode, bool transparent)
{
    switch (font_version)
    {
    case 2:
        return map_v2_color(pixel_value, dark_mode, transparent);
    
    case 3:
        return map_v3_color(pixel_value, dark_mode, transparent);
    
    default:
        // 未知版本，默认使用V2映射
        return map_v2_color(pixel_value, dark_mode, transparent);
    }
}

/**
 * @brief V2字体颜色映射 (1bit)
 * 
 * V2字体: 1bit编码
 * - 1 = 白色背景像素
 * - 0 = 黑色前景像素
 * 
 * Normal mode:
 * - 白色像素 -> 0xFFFF (白色背景) 或透明
 * - 黑色像素 -> 0x0000 (黑色文字)
 * 
 * Dark mode (反色):
 * - 白色像素 -> 0x0000 (黑色背景) 或透明
 * - 黑色像素 -> 0xFFFF (白色文字)
 */
uint16_t FontColorMapper::map_v2_color(uint8_t pixel_value, bool dark_mode, bool transparent)
{
    if (pixel_value == PIXEL_WHITE)
    {
        // 白色像素（背景）
        if (transparent)
        {
            return COLOR_TRANSPARENT;  // 透明标记
        }
        else
        {
            return dark_mode ? COLOR_BLACK : COLOR_WHITE;  // 根据模式返回背景色
        }
    }
    else // PIXEL_BLACK
    {
        // 黑色像素（前景文字）
        return dark_mode ? COLOR_WHITE : COLOR_BLACK;  // 根据模式返回前景色
    }
}

/**
 * @brief V3字体颜色映射 (2bit Huffman)
 * 
 * V3字体: 2bit Huffman编码
 * - 0 (single bit) = 白色背景像素
 * - 11 = 黑色前景像素
 * - 10 = 灰色像素
 * 
 * Normal mode:
 * - 白色像素(0) -> 0xFFFF 或透明
 * - 黑色像素(11) -> 0x0000
 * - 灰色像素(10) -> 0xAD55 (RGB565 灰度170，墨水屏约10-11级/16级)
 * 
 * Dark mode:
 * - 白色像素(0) -> 0x0000 或透明
 * - 黑色像素(11) -> 0xFFFF
 * - 灰色像素(10) -> 0x8C51 (RGB565 灰度136，墨水屏约8-9级/16级)
 */
uint16_t FontColorMapper::map_v3_color(uint8_t pixel_value, bool dark_mode, bool transparent)
{
    switch (pixel_value)
    {
    case PIXEL_WHITE:
        // 白色像素（背景）
        if (transparent)
        {
            return COLOR_TRANSPARENT;
        }
        else
        {
            return dark_mode ? COLOR_BLACK : COLOR_WHITE;
        }
    
    case PIXEL_BLACK:
        // 黑色像素（前景文字）
        return dark_mode ? COLOR_WHITE : COLOR_BLACK;
    
    case PIXEL_GRAY:
        // 灰色像素（抗锯齿）
        //return dark_mode ? COLOR_GRAY_DARK : COLOR_GRAY_LIGHT;
        // 打算粗暴点
        return dark_mode ? TFT_LIGHTGREY: GREY_MAP_COLOR;
    
    default:
        // 未知像素值，默认为透明
        return COLOR_TRANSPARENT;
    }
}

/**
 * @brief 获取背景色
 */
uint16_t FontColorMapper::get_background_color(bool dark_mode)
{
    return dark_mode ? COLOR_BLACK : COLOR_WHITE;
}

/**
 * @brief 获取前景色
 */
uint16_t FontColorMapper::get_foreground_color(bool dark_mode)
{
    return dark_mode ? COLOR_WHITE : COLOR_BLACK;
}
