#include <FS.h>
#include <SPIFFS.h>
#include "SD/SDWrapper.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <M5Unified.h>
#include "readpaper.h"
#include "readpaper.h"
#include "papers3.h"
#include "device/memory_pool.h"
#include "device/chunked_font_cache.h"
#include "test/per_file_debug.h"
#include "tasks/display_push_task.h"
#include "device/file_manager.h"
#include "../text/zh_conv.h"
// access per-book bookmark config
#include "book_handle.h"
#include "line_handle.h"
#include "font_decoder.h"
#include "bin_font_print.h"

// 检测是否为需要旋转的中文标点符号（与bin_font_print.cpp中保持一致）
static bool is_chinese_punctuation(uint32_t unicode)
{
    return (
        // unicode == 0x3001 || unicode == 0x3002 ||  // 、。
        //            unicode == 0xFF0C || unicode == 0xFF0E ||  // ，。
        unicode == 0xFF1A || unicode == 0xFF1B ||   // ：；
                                                    //            unicode == 0xFF1F || unicode == 0xFF01 ||  // ？！
        unicode == 0x300C || unicode == 0x300D ||   // 「」
        unicode == 0x300E || unicode == 0x300F ||   // 『』
        unicode == 0xFF08 || unicode == 0xFF09 ||   // （）
        unicode == 0x3010 || unicode == 0x3011 ||   // 【】
        unicode == 0x2018 || unicode == 0x2019 ||   // ''
        unicode == 0x201C || unicode == 0x201D ||   // ""
        unicode == 0x3008 || unicode == 0x3009 ||   // 〈〉
        unicode == 0x300A || unicode == 0x300B ||   // 《》
        unicode == 0x003C || unicode == 0x003E ||   // <>
        unicode == 0xFF1C || unicode == 0xFF1E ||   // ＜＞
        unicode == 0x2026 || unicode == 0x22EF ||   // …⋯
        unicode == 0x2025 || unicode == 0xFE19 ||   // ‥︙(两点/六点省略号)
        unicode == 0x005B || unicode == 0x005D ||   // []
        unicode == 0x0028 || unicode == 0x0029 ||   // ()
        unicode == 0x0024 || unicode == 0x0040 ||   // $@
        unicode == 0x002D || unicode == 0x005F ||   // -_
        unicode == 0x2014 || unicode == 0x003D ||   // —=
        unicode == 0x007E ||                        // ~
        (unicode >= 0x0030 && unicode <= 0x0039) || // 0-9
        (unicode >= 0x0041 && unicode <= 0x005A) || // A-Z
        (unicode >= 0x0061 && unicode <= 0x007A) || // a-z
        (unicode >= 0x00C0 && unicode <= 0x00FF) || // Latin-1 Supplement (À-ÿ)
        (unicode >= 0x0100 && unicode <= 0x017F) || // Latin Extended-A (Ā-ſ)
        (unicode >= 0x0180 && unicode <= 0x024F));  // Latin Extended-B (ƀ-ɏ)
}

int16_t calculate_text_width(const std::string &text, size_t start_pos, size_t end_pos)
{
    int16_t width = 0;
    const uint8_t *utf8 = (const uint8_t *)text.c_str() + start_pos;
    const uint8_t *end = (const uint8_t *)text.c_str() + std::min(end_pos, text.length());

    while (utf8 < end)
    {
        uint32_t unicode = utf8_decode(utf8, end);
        if (unicode == 0)
            break;

        bool glyph_exists = bin_font_has_glyph(unicode);
        if (glyph_exists)
        {
            width += bin_font_get_glyph_width(unicode);
        }
        else
        {
            width += bin_font_get_font_size() / 2;
        }
    }
    return width;
}

size_t find_break_position(const std::string &text, size_t start_pos, int16_t max_width, bool vertical, float scale_factor)
{
    size_t best_break = start_pos;
    size_t current_pos = start_pos;
    const uint8_t *utf8 = (const uint8_t *)text.c_str() + start_pos;
    const uint8_t *end = (const uint8_t *)text.c_str() + text.length();
    int16_t current_width = 0;

    while (utf8 < end)
    {
        const uint8_t *prev_utf8 = utf8;
        uint32_t unicode = utf8_decode(utf8, end);
        if (unicode == 0)
            break;

        if (unicode == '\n')
        {
            return utf8 - (const uint8_t *)text.c_str();
        }

        bool glyph_exists = bin_font_has_glyph(unicode);

        int16_t char_dimension;
        if (vertical)
        {
            // 竖排模式下，标点符号旋转后宽高互换
            if (is_chinese_punctuation(unicode) && glyph_exists)
            {
                // 标点符号旋转90度后，使用位图宽度(bitmapW)作为竖直方向的尺寸
                char_dimension = (int16_t)(bin_font_get_glyph_bitmapW(unicode) * scale_factor);
            }
            else
            {
                char_dimension = glyph_exists ? (int16_t)(bin_font_get_glyph_bitmapH(unicode) * scale_factor) : (int16_t)(bin_font_get_font_size() * scale_factor);
            }
        }
        else
        {
            char_dimension = glyph_exists ? (int16_t)(bin_font_get_glyph_width(unicode) * scale_factor) : (int16_t)(bin_font_get_font_size() * scale_factor / 2);
        }

        if (!glyph_exists || bin_font_get_glyph_bitmap_size(unicode) == 0)
        {
            // Match rendering fallback: use half of base font size scaled by scale_factor
            char_dimension = (int16_t)(bin_font_get_font_size() * scale_factor / 2);
        }

        int16_t char_spacing = vertical ? CHAR_SPACING_VERTICAL : (int16_t)(CHAR_SPACING_HORIZONTAL * scale_factor);

        if (current_width + char_dimension + char_spacing > max_width)
        {
            if (best_break > start_pos)
            {
                size_t piece_len = best_break - start_pos;
                if (piece_len <= 16)
                {
                    bool all_whitespace = true;
                    const uint8_t *check_start = (const uint8_t *)text.c_str() + start_pos;
                    const uint8_t *check_end = (const uint8_t *)text.c_str() + best_break;
                    const uint8_t *check_ptr = check_start;

                    while (check_ptr < check_end && all_whitespace)
                    {
                        uint32_t check_unicode = utf8_decode(check_ptr, check_end);
                        if (check_unicode != ' ' && check_unicode != '\t')
                        {
                            all_whitespace = false;
                        }
                    }

                    if (all_whitespace)
                    {
                        return prev_utf8 - (const uint8_t *)text.c_str();
                    }
                }
                return best_break;
            }
            return prev_utf8 - (const uint8_t *)text.c_str();
        }

        current_width += char_dimension + char_spacing;
        current_pos = utf8 - (const uint8_t *)text.c_str();

        if (unicode == ' ' || unicode == '\t' || unicode == '-')
        {
            if (current_pos > start_pos + 8)
            {
                best_break = current_pos;
            }
        }
    }

    return current_pos;
}

size_t find_break_position_scaled(const std::string &text, size_t start_pos, int16_t max_width, bool vertical, float font_size)
{
    float scale_factor = 1.0f;
    uint8_t base_font = get_font_size_from_file();
    if (font_size > 0 && base_font > 0)
    {
        scale_factor = font_size / (float)base_font;
    }
    return find_break_position(text, start_pos, max_width, vertical, scale_factor);
}
