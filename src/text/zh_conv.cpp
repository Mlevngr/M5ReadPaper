#include "zh_conv.h"
#include "../text/bin_font_print.h" // 添加这个头文件来包含 find_char 函数
#include "test/per_file_debug.h"
#include <map>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <Arduino.h>
#include <cstring>

// 添加字体查找函数的声明（使用C++链接以避免与实现产生链接冲突）
struct BinFontChar;
const BinFontChar *find_char(uint32_t unicode);

// Embedded lookup implemented in generated file (if present). Use C linkage.
// Provide a weak default implementation here so code links even if the
// generated file is not present; the real generated symbol will override
// this weak definition at link time when available.
extern "C" const char *zh_conv_embedded_lookup(const char *key, uint8_t mode) __attribute__((weak));
extern "C" const char *zh_conv_embedded_lookup(const char *key, uint8_t mode)
{
    (void)key;
    (void)mode;
    return nullptr;
}

// 仅嵌入模式：不再维护运行时映射表，以下占位变量保留以兼容调试输出。
static bool table_loaded = false;

void zh_conv_init()
{
    if (!table_loaded)
    {
#if ZH_CONV_DEBUG
        Serial.printf("zh_conv_init: embedded-only mode (no SPIFFS) freeHeap=%u\n", ESP.getFreeHeap());
#endif
        table_loaded = true;
    }
}

// Helper: trim whitespace from both ends
static inline std::string trim(const std::string &s)
{
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
        a++;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
        b--;
    return s.substr(a, b - a);
}

// SPIFFS 加载与运行时注册已移除。

// Very simple UTF-8 tokenization: since Chinese characters are typically 3-byte UTF-8,
// we attempt to match longest sequences in the table by greedily checking substrings up to a reasonable length (e.g., 6 characters)

std::string zh_conv_utf8(const std::string &in, uint8_t mode)
{
    //    mode = 2; // DONT CHANGE THIS @Coplot  , I am debugging..
    if (mode == 0)
    {
        // When mode==0 we do not perform conversions, but we still want to
        // replace characters that are not present in the current font with
        // U+25A1 (WHITE SQUARE) so missing glyphs are visible.
        std::string out0;
        size_t idx = 0;
        size_t in_n = in.size();
        while (idx < in_n)
        {
            unsigned char c = (unsigned char)in[idx];
            size_t char_len = 1;
            if ((c & 0x80) == 0)
                char_len = 1;
            else if ((c & 0xE0) == 0xC0)
                char_len = 2;
            else if ((c & 0xF0) == 0xE0)
                char_len = 3;
            else if ((c & 0xF8) == 0xF0)
                char_len = 4;
            if (idx + char_len > in_n)
                char_len = in_n - idx;

            std::string ch = in.substr(idx, char_len);

            // decode codepoint
            uint32_t cp = 0;
            unsigned char s0 = (unsigned char)ch[0];
            if ((s0 & 0x80) == 0)
            {
                cp = s0;
            }
            else if ((s0 & 0xE0) == 0xC0 && ch.size() >= 2)
            {
                cp = ((s0 & 0x1F) << 6) | ((unsigned char)ch[1] & 0x3F);
            }
            else if ((s0 & 0xF0) == 0xE0 && ch.size() >= 3)
            {
                cp = ((s0 & 0x0F) << 12) |
                     (((unsigned char)ch[1] & 0x3F) << 6) |
                     ((unsigned char)ch[2] & 0x3F);
            }
            else if ((s0 & 0xF8) == 0xF0 && ch.size() >= 4)
            {
                cp = ((s0 & 0x07) << 18) |
                     (((unsigned char)ch[1] & 0x3F) << 12) |
                     (((unsigned char)ch[2] & 0x3F) << 6) |
                     ((unsigned char)ch[3] & 0x3F);
            }

            if (cp != 0 && bin_font_has_glyph(cp))
            {
                out0 += ch;
            }
            else
            {
                // U+25A1 WHITE SQUARE as UTF-8: 0xE2 0x96 0xA1
                out0 += "\xE2\x96\xA1";
            }

            idx += char_len;
        }
        return out0;
    }
    if (!table_loaded)
    {
        // try init once
#if ZH_CONV_DEBUG
        Serial.printf("zh_conv_utf8: table not loaded, calling zh_conv_init() freeHeap=%u\n", ESP.getFreeHeap());
#endif
        zh_conv_init();
    }
    if (!table_loaded)
        return in; // no table, return original

#if ZH_CONV_DEBUG
    Serial.printf("zh_conv_utf8: after init table_loaded=%d (embedded-only, no runtime maps) freeHeap=%u\n", table_loaded ? 1 : 0, ESP.getFreeHeap());
#endif

    // 仅嵌入：不使用运行时 map（保留变量以兼容旧调试格式）
    // const void *frommap = nullptr; // unused - 已删除

    // Embedded lookup availability: top-level weak C symbol (zh_conv_embedded_lookup)
    bool has_embedded = true; // assume available; the weak symbol may return nullptr when absent

    // If runtime maps are empty we still proceed: the embedded lookup
    // (zh_conv_embedded_lookup) will be consulted inside the loop. Do not
    // short-circuit here — embedded-only builds rely on that behaviour.

#if ZH_CONV_DEBUG
    Serial.printf("zh_conv_utf8: in='%s' mode=%u (embedded lookup)\n", in.c_str(), (unsigned)mode);
#endif

    // Debug counters
    static uint32_t call_seq = 0;
    call_seq++;
    uint32_t emb_hits_mode1 = 0;
    uint32_t emb_hits_mode2 = 0;
    // uint32_t map_hits = 0; // unused - 已删除
    uint32_t copied_chars = 0;
    uint32_t converted_tokens = 0;
    if ((call_seq % 50) == 1)
    {
#if ZH_CONV_DEBUG
        Serial.printf("[zh_conv] call#%u mode=%u start len=%u (embedded only) heap=%u\n", call_seq, (unsigned)mode, (unsigned)in.size(), ESP.getFreeHeap());
#endif
        // Probe a few canonical keys both directions to inspect table direction.
        const char *probe_keys[] = {"剑", "劍", "剐", "剮", nullptr};
        for (int pk = 0; probe_keys[pk]; ++pk)
        {
            const char *k = probe_keys[pk];
            const char *p1 = zh_conv_embedded_lookup(k, 1);
            const char *p2 = zh_conv_embedded_lookup(k, 2);
#if ZH_CONV_DEBUG
            Serial.printf("[zh_conv][probe] key='%s' emb(1)='%s' emb(2)='%s'\n", k, p1 ? p1 : "", p2 ? p2 : "");
#endif
        }
    }

    std::string out;
    size_t i = 0;
    size_t n = in.size();

    unsigned replacements = 0;

    // Convert by longest-match greedy (max token bytes ~ 12 (4 chars * 3 bytes))
    const size_t MAX_TOKEN_BYTES = 36; // allow longer tokens if users include phrases

    while (i < n)
    {
        size_t max_j = std::min(n, i + MAX_TOKEN_BYTES);
        bool matched = false;
        // Try longer substrings first
        for (size_t j = max_j; j > i;)
        {
            size_t len = j - i;
            std::string sub = in.substr(i, len);
            // If the substring is already in the target script, keep it unchanged.
            // Note: when using embedded-only tables the runtime key sets may be empty,
            // so also consult zh_conv_embedded_lookup in the opposite mode to detect
            // whether 'sub' is already a key in the target-script table.
            // 运行时不再短路判断“已是目标脚本”——直接尝试嵌入查表。

            // First try embedded lookup (binary-search on flash/resident table)
            const char *emb = nullptr;
            if (has_embedded)
            {
                emb = zh_conv_embedded_lookup(sub.c_str(), mode);
                if (emb)
                {
                    if (mode == 1)
                        emb_hits_mode1++;
                    else
                        emb_hits_mode2++;
                }
            }
            if (emb)
            {
                // 检查转换结果在字体中是否映射到方框字符(0x25A1)
                // 这比检查UTF-8编码更准确，因为直接检查字体映射
                std::string emb_str(emb);

                // 检查转换后的字符在字体中的实际映射
                bool should_skip_conversion = false;

                // 对转换结果中的每个字符检查字体映射
                size_t check_pos = 0;
                while (check_pos < emb_str.length())
                {
                    // 解析UTF-8字符
                    unsigned char c = (unsigned char)emb_str[check_pos];
                    size_t char_len = 1;
                    uint32_t unicode = 0;

                    if ((c & 0x80) == 0)
                    {
                        char_len = 1;
                        unicode = c;
                    }
                    else if ((c & 0xE0) == 0xC0)
                    {
                        char_len = 2;
                        if (check_pos + 1 < emb_str.length())
                        {
                            unicode = ((c & 0x1F) << 6) | ((unsigned char)emb_str[check_pos + 1] & 0x3F);
                        }
                    }
                    else if ((c & 0xF0) == 0xE0)
                    {
                        char_len = 3;
                        if (check_pos + 2 < emb_str.length())
                        {
                            unicode = ((c & 0x0F) << 12) |
                                      (((unsigned char)emb_str[check_pos + 1] & 0x3F) << 6) |
                                      ((unsigned char)emb_str[check_pos + 2] & 0x3F);
                        }
                    }

                    // 检查这个Unicode字符在字体中是否存在
                    if (unicode > 0)
                    {
                        // 使用bin_font_print中的字体检查函数
                        // 如果字体中没有该字符，跳过转换使用原始字符
                        bool has_glyph = bin_font_has_glyph(unicode);
#if ZH_CONV_DEBUG
                        Serial.printf("[zh_conv][glyph_check] unicode=0x%04X has_glyph=%s\n", unicode, has_glyph ? "true" : "false");
#endif
                        if (!has_glyph)
                        {
#if ZH_CONV_DEBUG
                            Serial.printf("[zh_conv][skip_conv] 字符U+%04X在字体中不存在，跳过转换\n", unicode);
#endif
                            should_skip_conversion = true;
                            break;
                        }
                    }

                    check_pos += char_len;
                }

                if (should_skip_conversion)
                {
#if ZH_CONV_DEBUG
                    Serial.printf("[zh_conv][skip] mode=%u sub='%s' -> 字体中映射到方框字符，改为逐字符回退尝试\n", (unsigned)mode, sub.c_str());
#endif
                    // 逐字符回退尝试：对原始 sub 中每个 UTF-8 字符单独尝试转换并检查字体
                    size_t pos_sub = 0;
                    while (pos_sub < sub.size())
                    {
                        unsigned char c0 = (unsigned char)sub[pos_sub];
                        size_t ch_len = 1;
                        if ((c0 & 0x80) == 0)
                            ch_len = 1;
                        else if ((c0 & 0xE0) == 0xC0)
                            ch_len = 2;
                        else if ((c0 & 0xF0) == 0xE0)
                            ch_len = 3;
                        else if ((c0 & 0xF8) == 0xF0)
                            ch_len = 4;
                        if (pos_sub + ch_len > sub.size())
                            ch_len = sub.size() - pos_sub;

                        std::string char_src = sub.substr(pos_sub, ch_len);

                        const char *emb_char = nullptr;
                        if (has_embedded)
                            emb_char = zh_conv_embedded_lookup(char_src.c_str(), mode);

                        bool used_emb = false;
                        if (emb_char && emb_char[0] != '\0')
                        {
                            std::string emb_s(emb_char);
                            bool emb_all_ok = true;
                            size_t cp_pos = 0;
                            while (cp_pos < emb_s.size())
                            {
                                unsigned char cc = (unsigned char)emb_s[cp_pos];
                                size_t cp_len = 1;
                                uint32_t cp = 0;
                                if ((cc & 0x80) == 0)
                                {
                                    cp_len = 1;
                                    cp = cc;
                                }
                                else if ((cc & 0xE0) == 0xC0)
                                {
                                    cp_len = 2;
                                    if (cp_pos + 1 < emb_s.size())
                                        cp = ((cc & 0x1F) << 6) | ((unsigned char)emb_s[cp_pos + 1] & 0x3F);
                                }
                                else if ((cc & 0xF0) == 0xE0)
                                {
                                    cp_len = 3;
                                    if (cp_pos + 2 < emb_s.size())
                                        cp = ((cc & 0x0F) << 12) |
                                             (((unsigned char)emb_s[cp_pos + 1] & 0x3F) << 6) |
                                             ((unsigned char)emb_s[cp_pos + 2] & 0x3F);
                                }
                                else if ((cc & 0xF8) == 0xF0)
                                {
                                    cp_len = 4;
                                    if (cp_pos + 3 < emb_s.size())
                                        cp = ((cc & 0x07) << 18) |
                                             (((unsigned char)emb_s[cp_pos + 1] & 0x3F) << 12) |
                                             (((unsigned char)emb_s[cp_pos + 2] & 0x3F) << 6) |
                                             ((unsigned char)emb_s[cp_pos + 3] & 0x3F);
                                }

                                if (cp == 0 || !bin_font_has_glyph(cp))
                                {
                                    emb_all_ok = false;
                                    break;
                                }
                                cp_pos += cp_len;
                            }

                            if (emb_all_ok)
                            {
                                out += emb_s;
                                used_emb = true;
                            }
                        }

                        if (!used_emb)
                        {
                            // 保留原字符，除非字体也不包含该字符，若没有则用空白框 U+25A1 替换
                            // 解析 char_src 的 codepoint
                            uint32_t orig_cp = 0;
                            unsigned char f0 = (unsigned char)char_src[0];
                            if ((f0 & 0x80) == 0)
                            {
                                orig_cp = f0;
                            }
                            else if ((f0 & 0xE0) == 0xC0 && char_src.size() >= 2)
                            {
                                orig_cp = ((f0 & 0x1F) << 6) | ((unsigned char)char_src[1] & 0x3F);
                            }
                            else if ((f0 & 0xF0) == 0xE0 && char_src.size() >= 3)
                            {
                                orig_cp = ((f0 & 0x0F) << 12) |
                                          (((unsigned char)char_src[1] & 0x3F) << 6) |
                                          ((unsigned char)char_src[2] & 0x3F);
                            }
                            else if ((f0 & 0xF8) == 0xF0 && char_src.size() >= 4)
                            {
                                orig_cp = ((f0 & 0x07) << 18) |
                                          (((unsigned char)char_src[1] & 0x3F) << 12) |
                                          (((unsigned char)char_src[2] & 0x3F) << 6) |
                                          ((unsigned char)char_src[3] & 0x3F);
                            }

                            if (orig_cp != 0 && bin_font_has_glyph(orig_cp))
                            {
                                out += char_src;
                            }
                            else
                            {
                                // U+25A1 WHITE SQUARE as UTF-8: 0xE2 0x96 0xA1
                                out += "\xE2\x96\xA1";
                            }
                        }

                        pos_sub += ch_len;
                    }
                }
                else
                {
#if ZH_CONV_DEBUG
                    Serial.printf("[zh_conv][convert] mode=%u sub='%s' -> '%s'\n", (unsigned)mode, sub.c_str(), emb_str.c_str());
#endif
                    // 正常转换
                    out += emb_str;
                }

                i += len;
                matched = true;
                replacements++;
                converted_tokens++;
                if (len <= 6)
                {
#if ZH_CONV_DEBUG
                    Serial.printf("[zh_conv][conv][emb] mode=%u sub='%s' -> '%s' len=%u pos=%u\n", (unsigned)mode, sub.c_str(), emb, (unsigned)len, (unsigned)i);
#endif
                }
                break;
            }

            // 无运行时映射，跳过 map fallback 分支。
            // reduce j by one UTF-8 character (not byte)
            // step back to previous UTF-8 character boundary
            size_t k = j - 1;
            while (k > i && ((unsigned char)in[k] & 0xC0) == 0x80)
                k--; // skip continuation bytes
            j = k;
        }
        if (!matched)
        {
            // copy one UTF-8 character as-is, but replace with U+25A1 if font lacks the glyph
            unsigned char c = (unsigned char)in[i];
            size_t char_len = 1;
            if ((c & 0x80) == 0)
                char_len = 1;
            else if ((c & 0xE0) == 0xC0)
                char_len = 2;
            else if ((c & 0xF0) == 0xE0)
                char_len = 3;
            else if ((c & 0xF8) == 0xF0)
                char_len = 4;
            else
                char_len = 1;
            if (i + char_len > n)
                char_len = n - i;

            std::string single = std::string(in.data() + i, char_len);

            // parse codepoint
            uint32_t cp = 0;
            unsigned char s0 = (unsigned char)single[0];
            if ((s0 & 0x80) == 0)
            {
                cp = s0;
            }
            else if ((s0 & 0xE0) == 0xC0 && single.size() >= 2)
            {
                cp = ((s0 & 0x1F) << 6) | ((unsigned char)single[1] & 0x3F);
            }
            else if ((s0 & 0xF0) == 0xE0 && single.size() >= 3)
            {
                cp = ((s0 & 0x0F) << 12) |
                     (((unsigned char)single[1] & 0x3F) << 6) |
                     ((unsigned char)single[2] & 0x3F);
            }
            else if ((s0 & 0xF8) == 0xF0 && single.size() >= 4)
            {
                cp = ((s0 & 0x07) << 18) |
                     (((unsigned char)single[1] & 0x3F) << 12) |
                     (((unsigned char)single[2] & 0x3F) << 6) |
                     ((unsigned char)single[3] & 0x3F);
            }

            if (cp != 0 && bin_font_has_glyph(cp))
            {
                out += single;
            }
            else
            {
                out += "\xE2\x96\xA1"; // U+25A1
            }

            i += char_len;
            copied_chars++;
            if (char_len <= 4)
            {
                std::string copied = out.substr(out.size() - char_len, char_len);
#if ZH_CONV_DEBUG
                Serial.printf("[zh_conv][copy] mode=%u char='%s' pos=%u\n", (unsigned)mode, copied.c_str(), (unsigned)i);
#endif
            }
        }
    }

#if ZH_CONV_DEBUG
    Serial.printf("[zh_conv][summary] mode=%u emb1=%u emb2=%u copied=%u converted=%u inLen=%u outLen=%u\n",
                  (unsigned)mode, emb_hits_mode1, emb_hits_mode2, copied_chars, converted_tokens, (unsigned)in.size(), (unsigned)out.size());

#endif
    return out;
}
