/**
 * 页面字体缓存系统测试函数
 * 
 * 用于验证字体缓存系统的功能和性能
 * 可以在 debug 菜单或 state_debug_state.cpp 中调用
 */

#include "text/font_buffer.h"
#include "text/book_handle.h"
#include "current_book.h"
#include <Arduino.h>

// 测试函数：基本功能验证
void test_font_buffer_basic()
{
    Serial.println("\n========== Font Buffer Basic Test ==========");
    
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        Serial.println("ERROR: No book is currently open");
        return;
    }
    
    size_t current_page = book->getCurrentPageIndex();
    size_t total_pages = book->getTotalPages();
    
    Serial.printf("Book: %s\n", book->filePath().c_str());
    Serial.printf("Current page: %u / %u\n", (unsigned)current_page, (unsigned)total_pages);
    
    // 测试初始化
    Serial.println("\n[Test 1] Initializing font buffer manager...");
    unsigned long start = millis();
    bool init_ok = g_font_buffer_manager.initialize(book, current_page);
    unsigned long init_time = millis() - start;
    
    if (init_ok) {
        Serial.printf("✓ Initialization successful in %lu ms\n", init_time);
    } else {
        Serial.println("✗ Initialization failed");
        return;
    }
    
    // 检查各个缓存的有效性
    Serial.println("\n[Test 2] Cache validity check:");
    const char* cache_names[] = {"Page-2", "Page-1", "Current", "Page+1", "Page+2"};
    for (int offset = -2; offset <= 2; offset++) {
        bool valid = g_font_buffer_manager.isCacheValid(offset);
        Serial.printf("  %s (offset %+d): %s\n", 
                     cache_names[offset + 2], offset, 
                     valid ? "✓ Valid" : "✗ Invalid");
    }
    
    // 测试字符查询
    Serial.println("\n[Test 3] Character query test:");
    uint16_t test_chars[] = {
        0x4E2D,  // 中
        0x6587,  // 文
        0x0041,  // A
        0x0020,  // space
    };
    
    for (size_t i = 0; i < sizeof(test_chars) / sizeof(test_chars[0]); i++) {
        uint16_t unicode = test_chars[i];
        bool found = g_font_buffer_manager.hasChar(unicode, 0);
        
        Serial.printf("  U+%04X: %s", unicode, found ? "✓ Found" : "✗ Not found");
        
        if (found) {
            const CharGlyphInfo* info = g_font_buffer_manager.getCharGlyphInfo(unicode, 0);
            if (info) {
                Serial.printf(" [w=%u, h=%u×%u, size=%u B]", 
                             info->width, info->bitmapW, info->bitmapH, info->bitmap_size);
            }
        }
        Serial.println();
    }
    
    // 测试滚动更新
    if (current_page + 1 < total_pages) {
        Serial.println("\n[Test 4] Scroll update test (forward):");
        start = millis();
        bool scroll_ok = g_font_buffer_manager.scrollUpdate(book, current_page + 1, true);
        unsigned long scroll_time = millis() - start;
        
        if (scroll_ok) {
            Serial.printf("✓ Scroll update successful in %lu ms\n", scroll_time);
            Serial.printf("  New current page: %u\n", (unsigned)g_font_buffer_manager.getCurrentPageIndex());
        } else {
            Serial.println("✗ Scroll update failed");
        }
        
        // 恢复到原页面
        g_font_buffer_manager.scrollUpdate(book, current_page, false);
    }
    
    // 统计信息
    Serial.println("\n[Test 5] Memory usage:");
    size_t total_memory = 0;
    for (int offset = -2; offset <= 2; offset++) {
        int cache_idx = 2 + offset;
        // 注意：这里我们不能直接访问私有成员，所以只能估算
        // 实际使用中可以添加公共getter方法
        Serial.printf("  Cache[%d]: ", cache_idx);
        if (g_font_buffer_manager.isCacheValid(offset)) {
            Serial.println("Active");
            // 估算：假设每页约1000字符，每字符约100字节位图
            // 实际大小需要通过添加公共接口获取
        } else {
            Serial.println("Empty");
        }
    }
    
    Serial.println("\n[Test 6] Performance test:");
    uint16_t perf_char = 0x4E2D;
    start = micros();
    for (int i = 0; i < 10000; i++) {
        g_font_buffer_manager.hasChar(perf_char, 0);
    }
    unsigned long query_time = micros() - start;
    Serial.printf("  10000 queries: %lu us (avg %.2f us)\n", query_time, query_time / 10000.0);
    
    Serial.println("\n========== Test Complete ==========\n");
}

// 测试函数：翻页性能测试
void test_font_buffer_page_navigation()
{
    Serial.println("\n========== Font Buffer Page Navigation Test ==========");
    
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        Serial.println("ERROR: No book is currently open");
        return;
    }
    
    size_t start_page = book->getCurrentPageIndex();
    size_t total_pages = book->getTotalPages();
    
    if (total_pages < 10) {
        Serial.println("ERROR: Book too short for this test (need at least 10 pages)");
        return;
    }
    
    // 初始化缓存
    g_font_buffer_manager.initialize(book, start_page);
    
    Serial.printf("Testing %d consecutive page turns...\n", 5);
    
    unsigned long total_time = 0;
    unsigned long min_time = 999999;
    unsigned long max_time = 0;
    
    // 向前翻5页
    for (int i = 0; i < 5; i++) {
        size_t next_page = start_page + i + 1;
        if (next_page >= total_pages) break;
        
        unsigned long start = millis();
        bool ok = g_font_buffer_manager.scrollUpdate(book, next_page, true);
        unsigned long elapsed = millis() - start;
        
        total_time += elapsed;
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        
        Serial.printf("  Page %u -> %u: %s (%lu ms)\n", 
                     (unsigned)(next_page - 1), (unsigned)next_page,
                     ok ? "OK" : "FAIL", elapsed);
    }
    
    Serial.printf("\nStatistics:\n");
    Serial.printf("  Average: %.2f ms\n", total_time / 5.0);
    Serial.printf("  Min: %lu ms\n", min_time);
    Serial.printf("  Max: %lu ms\n", max_time);
    
    // 恢复原页面
    g_font_buffer_manager.initialize(book, start_page);
    
    Serial.println("\n========== Test Complete ==========\n");
}

// 测试函数：缓存命中率测试
void test_font_buffer_hit_rate()
{
    Serial.println("\n========== Font Buffer Hit Rate Test ==========");
    
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        Serial.println("ERROR: No book is currently open");
        return;
    }
    
    size_t current_page = book->getCurrentPageIndex();
    
    // 初始化缓存
    if (!g_font_buffer_manager.initialize(book, current_page)) {
        Serial.println("ERROR: Failed to initialize cache");
        return;
    }
    
    // 获取当前页内容
    TextPageResult page = book->currentPage();
    if (!page.success) {
        Serial.println("ERROR: Failed to read current page");
        return;
    }
    
    // 统计当前页的字符
    std::vector<uint16_t> page_chars;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(page.page_text.c_str());
    const uint8_t* end = p + page.page_text.size();
    
    while (p < end) {
        uint32_t unicode = 0;
        int bytes = 0;
        
        if (*p < 0x80) {
            unicode = *p;
            bytes = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            if (p + 1 < end) {
                unicode = ((*p & 0x1F) << 6) | (*(p+1) & 0x3F);
                bytes = 2;
            }
        } else if ((*p & 0xF0) == 0xE0) {
            if (p + 2 < end) {
                unicode = ((*p & 0x0F) << 12) | ((*(p+1) & 0x3F) << 6) | (*(p+2) & 0x3F);
                bytes = 3;
            }
        } else if ((*p & 0xF8) == 0xF0) {
            bytes = 4;
        }
        
        if (bytes > 0) {
            p += bytes;
            if (unicode > 0 && unicode <= 0xFFFF) {
                page_chars.push_back(static_cast<uint16_t>(unicode));
            }
        } else {
            p++;
        }
    }
    
    Serial.printf("Page has %u characters (including duplicates)\n", (unsigned)page_chars.size());
    
    // 测试缓存命中率
    int hits = 0;
    int misses = 0;
    
    for (uint16_t unicode : page_chars) {
        if (g_font_buffer_manager.hasChar(unicode, 0)) {
            hits++;
        } else {
            misses++;
        }
    }
    
    float hit_rate = (hits * 100.0f) / (hits + misses);
    
    Serial.printf("\nCache hit statistics:\n");
    Serial.printf("  Hits: %d\n", hits);
    Serial.printf("  Misses: %d\n", misses);
    Serial.printf("  Hit rate: %.1f%%\n", hit_rate);
    
    if (hit_rate < 99.0f) {
        Serial.println("\n⚠ Warning: Hit rate below 99% - possible issues with cache building");
    } else {
        Serial.println("\n✓ Excellent hit rate!");
    }
    
    Serial.println("\n========== Test Complete ==========\n");
}

// 清理测试：释放所有缓存
void test_font_buffer_cleanup()
{
    Serial.println("\n========== Font Buffer Cleanup Test ==========");
    
    Serial.println("Clearing all font caches...");
    g_font_buffer_manager.clearAll();
    
    // 验证清理
    bool all_cleared = true;
    for (int offset = -2; offset <= 2; offset++) {
        if (g_font_buffer_manager.isCacheValid(offset)) {
            all_cleared = false;
            Serial.printf("  Warning: Cache at offset %+d still valid\n", offset);
        }
    }
    
    if (all_cleared) {
        Serial.println("✓ All caches cleared successfully");
    } else {
        Serial.println("✗ Some caches not cleared");
    }
    
    Serial.println("\n========== Test Complete ==========\n");
}

// 主测试函数：运行所有测试
void run_all_font_buffer_tests()
{
    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════════════════╗");
    Serial.println("║   Font Buffer System Test Suite                  ║");
    Serial.println("╚═══════════════════════════════════════════════════╝");
    Serial.println();
    
    test_font_buffer_basic();
    delay(1000);
    
    test_font_buffer_hit_rate();
    delay(1000);
    
    test_font_buffer_page_navigation();
    delay(1000);
    
    test_font_buffer_cleanup();
    
    Serial.println("\n");
    Serial.println("╔═══════════════════════════════════════════════════╗");
    Serial.println("║   All Tests Complete                              ║");
    Serial.println("╚═══════════════════════════════════════════════════╝");
    Serial.println();
}

