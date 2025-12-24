#ifndef FONT_DECODER_H
#define FONT_DECODER_H

#include <stdint.h>
#include <cstddef>
#include <FS.h>

// Forward declarations
struct BinFontChar;

// 位图解码器
class FontDecoder
{
public:
    // 霍夫曼编码格式解码（原bin_font_generator.py）
    static void decode_bitmap(const uint8_t* raw_data, uint32_t bitmap_size, 
                             uint16_t* bitmap, int16_t w, int16_t h);
    
    static void decode_bitmap_transparent(const uint8_t* raw_data, uint32_t bitmap_size, 
                                         uint16_t* bitmap, int16_t w, int16_t h);
    
    // 1bit打包格式解码（generate_1bit_font_bin.py）
    static void decode_bitmap_1bit(const uint8_t* raw_data, uint32_t bitmap_size, 
                                  uint16_t* bitmap, int16_t w, int16_t h);
                                  
    static void decode_bitmap_1bit_transparent(const uint8_t* raw_data, uint32_t bitmap_size, 
                                              uint16_t* bitmap, int16_t w, int16_t h);
    
    // V3字体: 2bit Huffman编码格式解码
    // 编码规则: 0-白色, 11-黑色, 10-灰色
    static void decode_bitmap_v3(const uint8_t* raw_data, uint32_t bitmap_size,
                                uint16_t* bitmap, int16_t w, int16_t h,
                                bool dark_mode, bool transparent = false);
    
    static void decode_bitmap_v3_transparent(const uint8_t* raw_data, uint32_t bitmap_size,
                                            uint16_t* bitmap, int16_t w, int16_t h,
                                            bool dark_mode);
    
    static void draw_bitmap_direct(int16_t x, int16_t y, int16_t w, int16_t h, 
                                  uint32_t bitmap_offset, uint32_t bitmap_size);
                                  
    static void draw_bitmap_transparent(int16_t x, int16_t y, int16_t w, int16_t h, 
                                       const uint16_t* bitmap);
};

// UTF-8解码辅助函数
uint32_t utf8_decode(const uint8_t *&utf8, const uint8_t *end);

#endif // FONT_DECODER_H
