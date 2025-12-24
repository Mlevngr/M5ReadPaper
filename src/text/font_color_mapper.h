#ifndef FONT_COLOR_MAPPER_H
#define FONT_COLOR_MAPPER_H

#include <stdint.h>

/**
 * @brief 字体颜色映射器 - 支持多版本字体的颜色映射
 * 
 * 该类用于根据字体版本和dark mode状态，将解码后的像素值映射到实际的RGB565颜色值。
 * 
 * 支持的字体版本：
 * - V2: 1bit字体 (0=白色, 1=黑色)
 * - V3: 2bit Huffman编码字体 (0=白色, 11=黑色, 10=灰色)
 * - 未来可扩展更多版本
 */
class FontColorMapper
{
public:
    /**
     * @brief 像素值常量定义
     */
    enum PixelValue
    {
        PIXEL_WHITE = 0,      // 白色像素
        PIXEL_BLACK = 1,      // 黑色像素
        PIXEL_GRAY = 2,       // 灰色像素 (V3及以上)
        PIXEL_TRANSPARENT = 0xFFFF  // 透明像素标记
    };

    /**
     * @brief RGB565颜色常量
     * 
     * RGB565 格式的正确灰度值计算：
     * 对于 8-bit 灰度值 G：
     * - R5 = G >> 3 (取高5位)
     * - G6 = G >> 2 (取高6位)
     * - B5 = G >> 3 (取高5位)
     * - RGB565 = (R5 << 11) | (G6 << 5) | B5
     * 
     * 灰度 170 (0xAA): R=21, G=42, B=21 → 0xAD55 (墨水屏约10-11级/16级)
     * 灰度 136 (0x88): R=17, G=34, B=17 → 0x8C51 (墨水屏约8-9级/16级)
     */
    enum ColorRGB565
    {
        COLOR_WHITE = 0xFFFF,       // 纯白 (255,255,255)
        COLOR_BLACK = 0x0000,       // 纯黑 (0,0,0)
        COLOR_GRAY_LIGHT = 0xAD55,  // 浅灰 (normal mode) 灰度170 → 墨水屏约10-11级
        COLOR_GRAY_DARK = 0x8C51,   // 深灰 (dark mode) 灰度136 → 墨水屏约8-9级
        COLOR_TRANSPARENT = 0xF81F  // 透明标记 (粉红色)
    };

    /**
     * @brief 将像素值映射为RGB565颜色
     * 
     * @param pixel_value 解码后的像素值
     * @param font_version 字体版本号 (2=1bit, 3=2bit Huffman, ...)
     * @param dark_mode 是否为暗黑模式
     * @param transparent 是否使用透明背景模式
     * @return RGB565颜色值
     */
    static uint16_t map_pixel_to_color(uint8_t pixel_value, uint8_t font_version, 
                                      bool dark_mode, bool transparent = false);

    /**
     * @brief V2字体颜色映射 (1bit)
     * 
     * @param pixel_value 像素值 (0或1)
     * @param dark_mode 暗黑模式
     * @param transparent 透明模式
     * @return RGB565颜色
     */
    static uint16_t map_v2_color(uint8_t pixel_value, bool dark_mode, bool transparent);

    /**
     * @brief V3字体颜色映射 (2bit Huffman)
     * 
     * @param pixel_value 像素值 (0=白, 1=黑, 2=灰)
     * @param dark_mode 暗黑模式
     * @param transparent 透明模式
     * @return RGB565颜色
     */
    static uint16_t map_v3_color(uint8_t pixel_value, bool dark_mode, bool transparent);
    
    /**
     * @brief 获取背景色（用于非透明模式）
     * 
     * @param dark_mode 暗黑模式
     * @return RGB565颜色
     */
    static uint16_t get_background_color(bool dark_mode);
    
    /**
     * @brief 获取前景色（用于文本）
     * 
     * @param dark_mode 暗黑模式
     * @return RGB565颜色
     */
    static uint16_t get_foreground_color(bool dark_mode);
};

#endif // FONT_COLOR_MAPPER_H
