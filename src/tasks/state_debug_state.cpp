#include "state_machine_task.h"
#include "readpaper.h"
#include "state_debug.h"
#include "ui/show_debug.h"
#include "ui/ui_canvas_utils.h"
#include "ui/ui_lock_screen.h"
#include "device/internal_fs.h"
#include "SD/SDWrapper.h"
#include "device/ui_display.h"
#include "test/per_file_debug.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>

extern M5Canvas *g_canvas;

// Prefetch buffers used by STATE_DEBUG
static std::vector<uint8_t> g_stub_font; // 256 bytes pseudo-random
static std::vector<uint8_t> g_pref_lite; // header + char table from lite.bin (SPIFFS)
static std::vector<uint8_t> g_pref_sd;   // header + char table from SD FZLongZhaoJW.bin
static bool g_debug_prefetched = false;

// Helper: prefetch a font's header+char_table into vector; is_sd selects SDW::SD or provided fs
static void prefetch_font_from_fs(fs::FS *pfs, bool is_sd, const char *path, std::vector<uint8_t> &out)
{
    out.clear();
    File f;
    if (is_sd)
    {
        if (SDW::SD.cardSize() == 0)
            return;
        f = SDW::SD.open(path, "r");
    }
    else
    {
        if (!pfs)
            return;
        f = pfs->open(path, FILE_READ);
    }

    if (!f)
        return;

    // read header
    if (f.size() < 6)
    {
        f.close();
        return;
    }
    uint8_t header[6];
    f.seek(0);
    f.read(header, 6);
    uint32_t char_count = *(uint32_t *)&header[0];
    uint8_t version = header[5];

    size_t char_table_offset = 6;
    if (version >= 2)
        char_table_offset = 6 + 64 + 64;

    size_t table_size = (size_t)char_count * 20;
    size_t avail = 0;
    if (f.size() > char_table_offset)
        avail = f.size() - char_table_offset;

    size_t to_read = std::min(table_size, avail);

    // reserve: header + family+style (if present) + table portion
    size_t head_and_meta = (version >= 2) ? (6 + 64 + 64) : 6;
    size_t reserve_size = head_and_meta + to_read;
    // allocate buffer for header+meta+table portion; allocation failures will abort
    out.resize(reserve_size);

    // read header+meta
    f.seek(0);
    f.read(out.data(), head_and_meta);
    // read table portion
    if (to_read > 0)
    {
        f.seek(char_table_offset);
        size_t r2 = f.read(out.data() + head_and_meta, to_read);
        (void)r2;
    }

    f.close();
}

void StateMachineTask::handleDebugState(const SystemMessage_t *msg)
{
    // ensure prefetch on first entry
    if (!g_debug_prefetched)
    {
        // create stub_font 256 bytes pseudo-random
        g_stub_font.resize(256);
        unsigned long seed = micros();
        for (size_t i = 0; i < g_stub_font.size(); ++i)
        {
            seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
            g_stub_font[i] = (uint8_t)(seed & 0xFFu);
        }

        // prefetch lite.bin from InternalFS
        fs::FS &spiffs_fs = InternalFS::fs();
        prefetch_font_from_fs(&spiffs_fs, false, "/lite.bin", g_pref_lite);

        // prefetch SD font
        prefetch_font_from_fs(nullptr, true, "/font/FZLongZhaoJW.bin", g_pref_sd);

        g_debug_prefetched = true;
    }

    switch (msg->type)
    {
    case MSG_USER_ACTIVITY:
        // redraw debug UI on user activity
        break;
    case MSG_TOUCH_PRESSED:
    {
        int16_t tx = msg->data.touch.x;
        int16_t ty = msg->data.touch.y;
#if DBG_STATE_MACHINE_TASK
        sm_dbg_printf("DEBUG state touch (%d,%d)\n", tx, ty);
#endif
        if (debug_button_hit(0, tx, ty))
        {
            // SPIFFS test
            bool ok = InternalFS::isMounted() || InternalFS::begin(false);
            if (!ok)
            {
                Serial.println("A test: SPIFFS not mounted");
            }
            else
            {
                // Random-position read benchmark: pick a pseudo-random offset and read 256 bytes
                fs::FS &fs = InternalFS::fs();
                File f = fs.open("/lite.bin", FILE_READ);
                if (!f || f.size() == 0)
                {
                    Serial.println("A test: open /lite.bin failed or empty");
                }
                else
                {
                    const size_t RLEN = 112;
                    size_t fsize = f.size();
                    size_t offset = 0;
                    if (fsize > RLEN)
                    {
                        // simple pseudo-random offset using micros()
                        offset = (size_t)(micros() % (fsize - RLEN));
                    }

                    std::vector<uint8_t> rbuf(RLEN);
                    unsigned long t0 = micros();
                    f.seek(offset);
                    size_t got = f.read(rbuf.data(), RLEN);
                    unsigned long t1 = micros();
                    Serial.printf("A test: read at %u, got %u bytes, seek+read took %lu us\n", (unsigned)offset, (unsigned)got, (unsigned long)(t1 - t0));

                    // memory read from stub_font for comparison
                    if (g_stub_font.size() >= RLEN)
                    {
                        size_t stub_off = (g_stub_font.size() > RLEN) ? (size_t)(micros() % (g_stub_font.size() - RLEN)) : 0;
                        std::vector<uint8_t> membuf(RLEN);
                        unsigned long mt0 = micros();
                        memcpy(membuf.data(), g_stub_font.data() + stub_off, RLEN);
                        unsigned long mt1 = micros();
                        Serial.printf("A test: mem read at %u, copy %u bytes took %lu us\n", (unsigned)stub_off, (unsigned)RLEN, (unsigned long)(mt1 - mt0));
                    }

                    f.close();
                }
            }
        }
        else if (debug_button_hit(1, tx, ty))
        {
            // B测试开始前重置统计
            SDW::SD.reset_readAtOffset_stats();

            // SD test with read window: 测试预读窗口机制的性能
            bool ok = (SDW::SD.cardSize() > 0);
            if (!ok)
            {
                Serial.printf("B test: NO CARD\n");
            }
            else
            {
                const char *path = "/font/FZLongZhaoJW.bin";
                File sf = SDW::SD.open(path, "r");
                if (!sf || sf.size() == 0)
                {
                    Serial.printf("B test: open %s failed or empty\n", path);
                }
                else
                {
                    const size_t RLEN = 81;
                    size_t fsize = sf.size();
                    
                    // 预读窗口相关变量声明（无论是否启用都需要）
                    const size_t WINDOW_SIZE = 256 * 1024;
                    uint8_t *window_buffer = nullptr;
                    size_t window_offset = 0;
                    size_t window_valid = 0;
                    
#if ENABLE_PREREAD_WINDOW_IN_B_TEST
                    // 🚀 分配预读窗口（256KB PSRAM）
                    window_buffer = (uint8_t *)heap_caps_malloc(WINDOW_SIZE, MALLOC_CAP_SPIRAM);
                    if (!window_buffer) {
                        Serial.println("B test: 预读窗口分配失败，使用直接读取");
                    } else {
                        Serial.printf("B test: [预读窗口模式] 已分配 %uKB PSRAM 缓冲\n", WINDOW_SIZE / 1024);
                    }
#else
                    Serial.println("B test: [直接读取模式] 不使用预读窗口");
#endif
                    
                    // 🧪 性能测试：使用真实页面日志中的offset/size数据
                    const uint32_t real_offsets[] = {
                        1552562, 1722766, 1294351, 2526999, 1450916, 1299663, 1293131, 1328054, 1535478, 2001738,
                        1976169, 1294351, 2439713, 1591252, 1299663, 3262984, 1293131, 3098725, 1929622, 1942354,
                        2890083, 3031340, 613434, 1328054, 2400709, 2412054, 1381110, 1765341, 3283194, 1355150,
                        2367449, 1535478, 1293131, 1296853, 2287861, 1326638, 1293975, 3586723, 1449765, 1949736,
                        1535478, 2982307, 1399190, 1655065, 1949736, 2893069, 1946300, 1449414, 1293131, 1305851,
                        2287861, 2400709, 2526999, 1450916, 1943925, 1842332, 1320590, 3385463, 2412054, 2930814,
                        1665166, 3587160, 2128240, 1293975, 2400709, 3038584, 3101569, 2990563, 1946300, 1971736,
                        1972917, 2659948, 2835715, 2627635, 3096353, 3587173, 1293131, 2284217, 2131818, 2138380,
                        2990563, 1946300, 1972917, 1293131, 2688127, 2118120, 2145491, 2412998, 3105089, 3587173,
                        1976169, 2731026, 2792328, 2733691, 1807347, 1463162, 1391052, 1651585, 2400709, 2628059,
                        1982186, 613434, 1385712, 1327282, 2400709, 1293131, 1400328, 3130066, 1949736, 1661905,
                        3300004, 1941886, 1583853, 2400709, 3586723, 1450160, 1314978, 1306563, 2412998, 1971823,
                        1872248, 2400709, 3587173, 2001738, 1976169, 1465742, 2439713, 1591252, 1449765, 1949736,
                        2001738, 1976169, 1465742, 2439713, 1591252, 613434, 1938982, 3104457, 2626883, 1306151,
                        1848856, 1328054, 1726691, 1404533, 1305348, 1566897, 3383961, 1855624, 613434, 1328054,
                        2132122, 2420423, 1296371, 3094933, 1574379, 1293975, 1861796, 2190022, 1857548, 2284725,
                        2400709, 2891275, 1721650, 3031340, 1293131, 1326638, 1328054, 2400709, 1949736, 2001738,
                        1976169, 1465742, 2439713, 1591252, 3262984, 1293131, 3098725, 1929622, 1942354, 2890083,
                        3031340, 613434, 1328054, 2400709, 2412054, 1381110, 1765341, 3283194, 1355150, 2367449
                    };
                    const uint16_t real_sizes[] = {
                        112, 112, 81, 116, 116, 100, 16, 104, 112, 112,
                        112, 81, 100, 104, 100, 78, 16, 108, 108, 44,
                        108, 112, 9, 104, 78, 48, 108, 104, 112, 112,
                        81, 112, 16, 112, 104, 108, 96, 8, 69, 112,
                        112, 96, 78, 104, 112, 120, 104, 78, 16, 120,
                        104, 78, 116, 116, 104, 84, 108, 104, 48, 100,
                        120, 13, 112, 96, 78, 96, 108, 100, 104, 87,
                        116, 108, 116, 96, 108, 16, 16, 84, 96, 112,
                        100, 104, 116, 16, 104, 108, 100, 108, 104, 16,
                        112, 112, 116, 116, 116, 116, 112, 120, 78, 100,
                        120, 9, 116, 116, 78, 16, 100, 116, 112, 112,
                        112, 92, 87, 78, 8, 100, 88, 96, 108, 116,
                        108, 78, 16, 112, 112, 108, 100, 104, 69, 112,
                        112, 112, 108, 100, 104, 9, 100, 108, 104, 108,
                        104, 104, 120, 112, 78, 112, 78, 104, 9, 104,
                        96, 120, 72, 104, 112, 96, 108, 104, 108, 116,
                        78, 112, 116, 112, 16, 108, 104, 78, 112, 112,
                        112, 108, 100, 104, 78, 16, 108, 108, 44, 108,
                        112, 9, 104, 78, 48, 108, 104, 112, 112, 81
                    };
                    
                    Serial.println("B test: 开始随机读取测试（200次真实数据，使用预读窗口）...");
                    Serial.printf("B test: 文件大小 %u 字节\n", (unsigned)fsize);
                    const int TEST_COUNT = 200;
                    int window_hits = 0;
                    int window_repositions = 0;
                    int direct_reads = 0;
                    unsigned long total_time_us = 0;
                    unsigned long test_start = micros();
                    
                    for (int i = 0; i < TEST_COUNT; i++)
                    {
                        size_t test_offset = real_offsets[i % 200];
                        size_t test_size = real_sizes[i % 200];
                        
                        // 使用模运算将offset映射到有效范围内（模拟真实页面读取访问模式）
                        test_offset = test_offset % fsize;
                        if (test_offset + test_size > fsize) {
                            test_size = fsize - test_offset;  // 截断size以避免越界
                        }
                        
                        if (test_size == 0) continue;  // 跳过无效的大小
                        
                        std::vector<uint8_t> read_buf(test_size);
                        bool read_success = false;
                        unsigned long iter_start = micros();
                        
                        // 尝试从预读窗口读取
                        if (window_buffer && window_valid > 0)
                        {
                            // 检查是否在窗口内
                            if (test_offset >= window_offset && 
                                test_offset + test_size <= window_offset + window_valid)
                            {
                                // 命中！从窗口复制
                                size_t offset_in_window = test_offset - window_offset;
                                memcpy(read_buf.data(), window_buffer + offset_in_window, test_size);
                                window_hits++;
                                read_success = true;
                            }
                        }
                        
                        // 未命中，需要读取
                        if (!read_success)
                        {
                            if (window_buffer)
                            {
                                // 重新定位窗口
                                sf.seek(test_offset);
                                window_valid = sf.read(window_buffer, WINDOW_SIZE);
                                window_offset = test_offset;
                                
                                if (window_valid >= test_size) {
                                    memcpy(read_buf.data(), window_buffer, test_size);
                                    window_repositions++;
                                    read_success = true;
                                }
                            }
                            
                            if (!read_success) {
                                // 窗口失败，直接读取
                                size_t got = SDW::SD.readAtOffset(sf, test_offset, read_buf.data(), test_size);
                                direct_reads++;
                                read_success = (got == test_size);
                            }
                        }
                        
                        unsigned long iter_time = micros() - iter_start;
                        total_time_us += iter_time;
                        
                        if (!read_success) {
                            Serial.printf("B test: 第 %d 次读取失败 (offset=%u size=%u)\n", 
                                         i + 1, (unsigned)test_offset, (unsigned)test_size);
                            break;
                        }
                    }
                    
                    unsigned long test_total = micros() - test_start;
                    Serial.printf("B test: 随机读取测试完成\n");
#if ENABLE_PREREAD_WINDOW_IN_B_TEST
                    Serial.printf("B test: [预读窗口结果] 命中=%d 重定位=%d 直接读取=%d\n", 
                                 window_hits, window_repositions, direct_reads);
#else
                    Serial.printf("B test: [直接读取结果] 所有读取均通过readAtOffset()\n");
#endif
                    Serial.printf("B test: 总耗时=%lu us, 平均每次=%lu us, 测量总和=%lu us\n",
                                 (unsigned long)test_total, (unsigned long)(test_total / TEST_COUNT), 
                                 (unsigned long)total_time_us);
                    
                    // 打印readAtOffset统计信息
                    SDW::SD.print_readAtOffset_stats();
                    
                    // 清理
                    if (window_buffer) {
                        heap_caps_free(window_buffer);
                    }
                    sf.close();
                }
            }
        }
        else if (debug_button_hit(2, tx, ty))
        {
            // C test: locate glyph for '亮' (U+4EAE) in both lite.bin (SPIFFS) and
            // FZLongZhaoJW.bin (SD), then time a seek+read of the glyph bitmap.
            const uint16_t target_unicode = 0x4EAE; // '亮'

            auto find_and_time = [&](fs::FS *pfs, const char *path, bool is_sd) -> void
            {
                if (!pfs && !is_sd)
                {
                    Serial.printf("C test: no fs for %s\n", path);
                    return;
                }
                File f;
                if (is_sd)
                    f = SDW::SD.open(path, "r");
                else
                    f = pfs->open(path, FILE_READ);

                if (!f || f.size() < 6)
                {
                    Serial.printf("C test: open %s failed or too small\n", path);
                    return;
                }

                // Try to resolve glyph using prefetched table if available
                bool found = false;
                uint32_t bitmap_offset = 0;
                uint32_t bitmap_size = 0;
                uint16_t bitmapW = 0, bitmapH = 0;

                const std::vector<uint8_t> *pref = nullptr;
                if (!is_sd && g_pref_lite.size() > 6)
                    pref = &g_pref_lite;
                else if (is_sd && g_pref_sd.size() > 6)
                    pref = &g_pref_sd;

                if (pref)
                {
                    // parse header from prefetched buffer
                    const uint8_t *p = pref->data();
                    uint8_t version = p[5];
                    size_t head_and_meta = (version >= 2) ? (6 + 64 + 64) : 6;
                    if (pref->size() >= head_and_meta)
                    {
                        size_t table_len = pref->size() - head_and_meta;
                        size_t avail_entries = table_len / 20;
                        for (size_t i = 0; i < avail_entries; ++i)
                        {
                            const uint8_t *entry = p + head_and_meta + i * 20;
                            uint16_t unicode = *(uint16_t *)&entry[0];
                            if (unicode == target_unicode)
                            {
                                bitmapW = (uint16_t)entry[4];
                                bitmapH = (uint16_t)entry[5];
                                bitmap_offset = *(uint32_t *)&entry[8];
                                bitmap_size = *(uint32_t *)&entry[12];
                                found = true;
                                break;
                            }
                        }
                    }
                }

                if (!found)
                {
                    // fallback to scanning the file's char table
                    // Read header (6 bytes)
                    f.seek(0);
                    uint8_t header[6];
                    f.read(header, 6);
                    uint32_t char_count = *(uint32_t *)&header[0];
                    uint8_t version = header[5];
                    size_t char_table_offset = 6;
                    if (version >= 2)
                        char_table_offset = 6 + 64 + 64;

                    if (char_count == 0 || f.size() < char_table_offset)
                    {
                        Serial.printf("C test: %s has invalid char table\n", path);
                        f.close();
                        return;
                    }

                    f.seek(char_table_offset);
                    for (uint32_t i = 0; i < char_count; ++i)
                    {
                        uint8_t entry[20];
                        size_t r = f.read(entry, 20);
                        if (r != 20)
                            break;
                        uint16_t unicode = *(uint16_t *)&entry[0];
                        if (unicode == target_unicode)
                        {
                            bitmapW = (uint16_t)entry[4];
                            bitmapH = (uint16_t)entry[5];
                            bitmap_offset = *(uint32_t *)&entry[8];
                            bitmap_size = *(uint32_t *)&entry[12];
                            found = true;
                            break;
                        }
                    }
                }

                if (!found)
                {
                    Serial.printf("C test: glyph U+%04X not found in %s\n", target_unicode, path);
                    f.close();
                    return;
                }

                // limit read size to something reasonable
                size_t to_read = bitmap_size;
                const size_t MAX_READ = 256 * 1024; // 256KB cap
                if (to_read > MAX_READ)
                    to_read = MAX_READ;

                // allocate buffer and time seek+read (only measure seek+read)
                std::vector<uint8_t> rb(to_read);
                unsigned long t0 = micros();
                f.seek((size_t)bitmap_offset);
                size_t got = f.read(rb.data(), to_read);
                unsigned long t1 = micros();
                Serial.printf("C test: %s glyph found: bitmapW=%u,H=%u,size=%u, read %u bytes, seek+read %lu us\n",
                              path, (unsigned)bitmapW, (unsigned)bitmapH, (unsigned)bitmap_size, (unsigned)got, (unsigned long)(t1 - t0));

                f.close();
            };

            // SPIFFS: use InternalFS
            fs::FS &spiffs_fs = InternalFS::fs();
            find_and_time(&spiffs_fs, "/lite.bin", false);

            // SD: use SD wrapper
            find_and_time(nullptr, "/font/FZLongZhaoJW.bin", true);
        }
        break;
    }
    case MSG_TOUCH_RELEASED:
        // ignore for now
        break;
    default:
        // ignore other messages
        break;
    }
}
