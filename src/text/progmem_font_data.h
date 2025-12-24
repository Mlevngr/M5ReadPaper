#pragma once

#include <pgmspace.h>
#include <stdint.h>
#include <cstring>

// PROGMEM 字体数据头文件
// 本文件由字体生成脚本自动生成，包含编译进 Flash 的字体数据
// 使用方式：当 fontLoadLoc == 1 时，从 PROGMEM 读取字体数据

// 字体文件格式版本 2 的结构：
// Header: 
//   - uint32_t char_count      (4 bytes)
//   - uint8_t font_height      (1 byte)
//   - uint8_t version          (1 byte)
//   - char family_name[64]     (64 bytes, UTF-8)
//   - char style_name[64]      (64 bytes, UTF-8)
// Entry (每个字符 20 bytes):
//   - uint16_t unicode         (2 bytes)
//   - uint16_t width           (2 bytes)
//   - uint8_t bitmapW          (1 byte)
//   - uint8_t bitmapH          (1 byte)
//   - int8_t x_offset          (1 byte)
//   - int8_t y_offset          (1 byte)
//   - uint32_t bitmap_offset   (4 bytes)
//   - uint32_t bitmap_size     (4 bytes)
//   - uint32_t cached_bitmap   (4 bytes, reserved)
// Bitmap Data: 所有字形的 1-bit 打包位图数据

// 全局标志：是否存在有效的 PROGMEM 字体数据
extern const bool g_has_progmem_font;

// 字体数据总大小（用于边界检查）
extern const uint32_t g_progmem_font_size;

// PROGMEM 字体数据（存储在 Flash）
extern const uint8_t g_progmem_font_data[] PROGMEM;

// 辅助函数：从 PROGMEM 读取单字节
inline uint8_t progmem_read_byte(uint32_t offset) {
    if (offset >= g_progmem_font_size) {
        return 0;
    }
    return pgm_read_byte(&g_progmem_font_data[offset]);
}

// 辅助函数：从 PROGMEM 读取多字节到缓冲区
inline void progmem_read_buffer(uint32_t offset, uint8_t* buffer, uint32_t size) {
    if (offset + size > g_progmem_font_size) {
        // 防止越界
        size = (offset < g_progmem_font_size) ? (g_progmem_font_size - offset) : 0;
    }
    if (size > 0) {
        memcpy_P(buffer, &g_progmem_font_data[offset], size);
    }
}

// 辅助函数：从 PROGMEM 读取 uint32_t
inline uint32_t progmem_read_uint32(uint32_t offset) {
    uint32_t value = 0;
    if (offset + 4 <= g_progmem_font_size) {
        // 使用 memcpy_P 读取4字节，然后解析为 uint32_t
        uint8_t bytes[4];
        memcpy_P(bytes, &g_progmem_font_data[offset], 4);
        value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
    }
    return value;
}

// 辅助函数：从 PROGMEM 读取 uint16_t
inline uint16_t progmem_read_uint16(uint32_t offset) {
    uint16_t value = 0;
    if (offset + 2 <= g_progmem_font_size) {
        uint8_t bytes[2];
        memcpy_P(bytes, &g_progmem_font_data[offset], 2);
        value = bytes[0] | (bytes[1] << 8);
    }
    return value;
}

// 辅助函数：从 PROGMEM 读取 int8_t
inline int8_t progmem_read_int8(uint32_t offset) {
    if (offset >= g_progmem_font_size) {
        return 0;
    }
    return (int8_t)pgm_read_byte(&g_progmem_font_data[offset]);
}

// 默认实现：空数据（占位符）
// 实际字体数据由生成脚本创建的 .cpp 文件提供
// 外部声明（定义在 lite.cpp 或其他生成的 .cpp 文件中）
extern const bool g_has_progmem_font;
extern const uint32_t g_progmem_font_size;
extern const uint8_t g_progmem_font_data[];
