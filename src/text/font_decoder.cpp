#include "font_decoder.h"
#include "font_color_mapper.h"
#include "device/memory_pool.h"
#include <M5Unified.h>
#include "test/per_file_debug.h"

// 声明外部变量
extern M5Canvas *g_canvas;

void FontDecoder::decode_bitmap_1bit(const uint8_t* raw_data, uint32_t bitmap_size, 
                                    uint16_t* bitmap, int16_t w, int16_t h)
{
    // 1bit格式解码：每个像素1bit，按行优先，每行补齐到字节边界
    for (int i = 0; i < w * h; i++) {
        bitmap[i] = 0xFFFF; // 默认白色
    }
    
    int pixel_idx = 0;
    for (int y = 0; y < h; y++) {
        int bytes_per_row = (w + 7) / 8;
        int byte_start = y * bytes_per_row;
        
        for (int x = 0; x < w; x++) {
            if (pixel_idx >= w * h) break;
            
            int byte_idx = byte_start + x / 8;
            int bit_idx = 7 - (x % 8);
            
            if (byte_idx < bitmap_size) {
                uint8_t pixel = (raw_data[byte_idx] >> bit_idx) & 1;
                bitmap[pixel_idx] = pixel ? 0xFFFF : 0x0000; // 反转：1=白色, 0=黑色
            }
            pixel_idx++;
        }
    }
}

void FontDecoder::decode_bitmap_1bit_transparent(const uint8_t* raw_data, uint32_t bitmap_size, 
                                                uint16_t* bitmap, int16_t w, int16_t h)
{
    // 1bit格式透明版本：白色像素设为透明
    for (int i = 0; i < w * h; i++) {
        bitmap[i] = 0xF81F; // 透明标记
    }
    
    int pixel_idx = 0;
    for (int y = 0; y < h; y++) {
        int bytes_per_row = (w + 7) / 8;
        int byte_start = y * bytes_per_row;
        
        for (int x = 0; x < w; x++) {
            if (pixel_idx >= w * h) break;
            
            int byte_idx = byte_start + x / 8;
            int bit_idx = 7 - (x % 8);
            
            if (byte_idx < bitmap_size) {
                uint8_t pixel = (raw_data[byte_idx] >> bit_idx) & 1;
                if (!pixel) { // 原本是0的设为黑色（反转逻辑）
                    bitmap[pixel_idx] = 0x0000;
                }
                // 原本是1的像素保持透明(0xF81F)
            }
            pixel_idx++;
        }
    }
}

void FontDecoder::decode_bitmap(const uint8_t* raw_data, uint32_t bitmap_size, 
                               uint16_t* bitmap, int16_t w, int16_t h)
{
    // 位图初始化
    for (int i = 0; i < w * h; i++)
    {
        bitmap[i] = 0xFFFF; // 白色16位色彩
    }

    // 解码位图数据 - 严格按照Python编码逻辑
    size_t pixel_idx = 0;
    size_t bit_pos = 0;
    size_t byte_pos = 0;

    while (pixel_idx < w * h && byte_pos < bitmap_size)
    {
        if (bit_pos >= 8)
        {
            bit_pos = 0;
            byte_pos++;
            if (byte_pos >= bitmap_size)
                break;
        }

        uint8_t current_byte = raw_data[byte_pos];
        uint8_t bit = (current_byte >> (7 - bit_pos)) & 0x01;

        if (bit == 0)
        {
            // 白色像素 (编码为'0')
            bitmap[pixel_idx++] = 0xFFFF; // 白色
            bit_pos++;
        }
        else
        {
            // 检查下一位
            bit_pos++;
            if (bit_pos >= 8)
            {
                bit_pos = 0;
                byte_pos++;
                if (byte_pos >= bitmap_size)
                    break;
                current_byte = raw_data[byte_pos];
            }

            bit = (current_byte >> (7 - bit_pos)) & 0x01;

            if (bit == 0)
            {
                // 黑色像素 (编码为'10')
                bitmap[pixel_idx++] = 0x0000; // 黑色
                bit_pos++;
            }
            else
            {
                // 灰度像素 (编码为'11'加4位灰度值)
                // 按照Python的bin(v)[2:].zfill(4)逻辑解码
                uint8_t gray_value = 0;
                bit_pos++; // 跳过第二个'1'

                // 读取4位灰度值（高位在前）
                for (int i = 0; i < 4; i++)
                {
                    if (bit_pos >= 8)
                    {
                        bit_pos = 0;
                        byte_pos++;
                        if (byte_pos >= bitmap_size)
                            break;
                        current_byte = raw_data[byte_pos];
                    }
                    if (byte_pos < bitmap_size)
                    {
                        uint8_t bit_val = (current_byte >> (7 - bit_pos)) & 0x01;
                        gray_value = (gray_value << 1) | bit_val;
                        bit_pos++;
                    }
                }

                // 将4位灰度值转换为16位灰度色彩
                // Python逻辑：quantized[mid_mask] = (black_threshold - cropped_area[mid_mask]) // 14
                // 所以gray_value越大表示越暗
                uint8_t gray_8bit = (15 - gray_value) * 17; // 转换为8位
                uint16_t gray_16bit = (gray_8bit >> 3) << 11 | (gray_8bit >> 2) << 5 | (gray_8bit >> 3);
                bitmap[pixel_idx++] = gray_16bit;
            }
        }
    }
}

// ==================== V3字体解码器 (2bit Huffman编码) ====================

/**
 * @brief V3字体解码 - 2bit Huffman编码
 * 
 * 编码规则:
 * - 0 (single bit) = 白色背景像素
 * - 11 = 黑色前景像素
 * - 10 = 灰色像素（抗锯齿）
 * 
 * @param raw_data 原始位图数据
 * @param bitmap_size 位图数据大小
 * @param bitmap 输出的RGB565位图
 * @param w 宽度
 * @param h 高度
 * @param dark_mode 是否为暗黑模式
 * @param transparent 是否使用透明背景
 */
void FontDecoder::decode_bitmap_v3(const uint8_t* raw_data, uint32_t bitmap_size,
                                  uint16_t* bitmap, int16_t w, int16_t h,
                                  bool dark_mode, bool transparent)
{
    // 初始化位图
    uint16_t bg_color = transparent ? FontColorMapper::COLOR_TRANSPARENT 
                                   : FontColorMapper::get_background_color(dark_mode);
    for (int i = 0; i < w * h; i++)
    {
        bitmap[i] = bg_color;
    }

    // 解码Huffman编码的位图数据
    size_t pixel_idx = 0;
    size_t bit_pos = 0;
    size_t byte_pos = 0;

    while (pixel_idx < w * h && byte_pos < bitmap_size)
    {
        // 确保bit_pos在有效范围内
        if (bit_pos >= 8)
        {
            bit_pos = 0;
            byte_pos++;
            if (byte_pos >= bitmap_size)
                break;
        }

        uint8_t current_byte = raw_data[byte_pos];
        uint8_t first_bit = (current_byte >> (7 - bit_pos)) & 0x01;

        if (first_bit == 0)
        {
            // 编码为'0' - 白色背景像素
            uint8_t pixel_value = FontColorMapper::PIXEL_WHITE;
            uint16_t color = FontColorMapper::map_v3_color(pixel_value, dark_mode, transparent);
            bitmap[pixel_idx++] = color;
            bit_pos++;
        }
        else
        {
            // 第一位是'1'，需要读取第二位
            bit_pos++;
            if (bit_pos >= 8)
            {
                bit_pos = 0;
                byte_pos++;
                if (byte_pos >= bitmap_size)
                    break;
                current_byte = raw_data[byte_pos];
            }

            uint8_t second_bit = (current_byte >> (7 - bit_pos)) & 0x01;
            bit_pos++;

            uint8_t pixel_value;
            if (second_bit == 0)
            {
                // 编码为'10' - 灰色像素
                pixel_value = FontColorMapper::PIXEL_GRAY;
            }
            else
            {
                // 编码为'11' - 黑色前景像素
                pixel_value = FontColorMapper::PIXEL_BLACK;
            }

            uint16_t color = FontColorMapper::map_v3_color(pixel_value, dark_mode, transparent);
            bitmap[pixel_idx++] = color;
        }
    }
}

/**
 * @brief V3字体透明版本解码
 * 
 * 这是decode_bitmap_v3的透明版本，白色像素会被设置为透明
 */
void FontDecoder::decode_bitmap_v3_transparent(const uint8_t* raw_data, uint32_t bitmap_size,
                                              uint16_t* bitmap, int16_t w, int16_t h,
                                              bool dark_mode)
{
    decode_bitmap_v3(raw_data, bitmap_size, bitmap, w, h, dark_mode, true);
}

void FontDecoder::draw_bitmap_direct(int16_t x, int16_t y, int16_t w, int16_t h, 
                                    uint32_t bitmap_offset, uint32_t bitmap_size)
{
    // 这个函数暂时不实现，因为需要全局文件句柄
#if DBG_FONT_DECODER
    Serial.printf("[FontDecoder] draw_bitmap_direct 暂未实现\n");
#endif
}

uint32_t utf8_decode(const uint8_t *&utf8, const uint8_t *end)
{
    if (utf8 >= end)
        return 0;

    uint8_t first = *utf8++;
    if ((first & 0x80) == 0)
    {
        // ASCII字符
        return first;
    }
    else if ((first & 0xE0) == 0xC0)
    {
        // 2字节UTF-8
        if (utf8 >= end)
            return 0;
        return ((first & 0x1F) << 6) | (*utf8++ & 0x3F);
    }
    else if ((first & 0xF0) == 0xE0)
    {
        // 3字节UTF-8
        if (utf8 + 1 >= end)
            return 0;
        uint32_t result = (first & 0x0F) << 12;
        result |= (*utf8++ & 0x3F) << 6;
        result |= (*utf8++ & 0x3F);
        return result;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        // 4字节UTF-8
        if (utf8 + 2 >= end)
            return 0;
        uint32_t result = (first & 0x07) << 18;
        result |= (*utf8++ & 0x3F) << 12;
        result |= (*utf8++ & 0x3F) << 6;
        result |= (*utf8++ & 0x3F);
        return result;
    }
    return 0; // 无效字符
}

void FontDecoder::decode_bitmap_transparent(const uint8_t* raw_data, uint32_t bitmap_size, 
                                           uint16_t* bitmap, int16_t w, int16_t h)
{
    // 位图初始化 - 透明版本使用特殊值标记透明像素
    for (int i = 0; i < w * h; i++)
    {
        bitmap[i] = 0xF81F; // 使用特殊颜色值标记透明像素 (粉红色，通常不用)
    }

    // 解码位图数据 - 严格按照Python编码逻辑
    size_t pixel_idx = 0;
    size_t bit_pos = 0;
    size_t byte_pos = 0;

    while (pixel_idx < w * h && byte_pos < bitmap_size)
    {
        if (bit_pos >= 8)
        {
            bit_pos = 0;
            byte_pos++;
            if (byte_pos >= bitmap_size)
                break;
        }

        uint8_t current_byte = raw_data[byte_pos];
        uint8_t bit = (current_byte >> (7 - bit_pos)) & 0x01;

        if (bit == 0)
        {
            // 白色像素 (编码为'0') - 设置为透明
            bitmap[pixel_idx++] = 0xF81F; // 保持透明标记
            bit_pos++;
        }
        else
        {
            // 检查下一位
            bit_pos++;
            if (bit_pos >= 8)
            {
                bit_pos = 0;
                byte_pos++;
                if (byte_pos >= bitmap_size)
                    break;
                current_byte = raw_data[byte_pos];
            }

            bit = (current_byte >> (7 - bit_pos)) & 0x01;

            if (bit == 0)
            {
                // 黑色像素 (编码为'10')
                bitmap[pixel_idx++] = 0x0000; // 黑色
                bit_pos++;
            }
            else
            {
                // 灰度像素 (编码为'11'加4位灰度值)
                uint8_t gray_value = 0;
                bit_pos++; // 跳过第二个'1'

                // 读取4位灰度值（高位在前）
                for (int i = 0; i < 4; i++)
                {
                    if (bit_pos >= 8)
                    {
                        bit_pos = 0;
                        byte_pos++;
                        if (byte_pos >= bitmap_size)
                            break;
                        current_byte = raw_data[byte_pos];
                    }
                    if (byte_pos < bitmap_size)
                    {
                        uint8_t bit_val = (current_byte >> (7 - bit_pos)) & 0x01;
                        gray_value = (gray_value << 1) | bit_val;
                        bit_pos++;
                    }
                }

                // 将4位灰度值转换为16位灰度色彩
                uint8_t gray_8bit = (15 - gray_value) * 17; // 转换为8位
                uint16_t gray_16bit = (gray_8bit >> 3) << 11 | (gray_8bit >> 2) << 5 | (gray_8bit >> 3);
                bitmap[pixel_idx++] = gray_16bit;
            }
        }
    }
}

void FontDecoder::draw_bitmap_transparent(int16_t x, int16_t y, int16_t w, int16_t h, 
                                         const uint16_t* bitmap)
{
    if (!g_canvas || !bitmap) return;
    
    // 逐像素绘制，跳过透明像素
    for (int16_t py = 0; py < h; py++)
    {
        for (int16_t px = 0; px < w; px++)
        {
            uint16_t pixel = bitmap[py * w + px];
            if (pixel != 0xF81F)  // 不是透明像素
            {
                int16_t screen_x = x + px;
                int16_t screen_y = y + py;
                
                // 边界检查
                if (screen_x >= 0 && screen_x < g_canvas->width() && 
                    screen_y >= 0 && screen_y < g_canvas->height())
                {
                    g_canvas->drawPixel(screen_x, screen_y, pixel);
                }
            }
        }
    }
}
