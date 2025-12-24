/**
 * 页面字体缓存系统使用示例
 * 
 * 这个文件展示了如何使用 FontBufferManager 来管理页面字体缓存
 */

#include "text/font_buffer.h"
#include "text/book_handle.h"
#include "current_book.h"
#include <Arduino.h>

// ========== 基本使用示例 ==========

void example_basic_usage()
{
    // 获取当前打开的书籍
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        Serial.println("No book is currently open");
        return;
    }
    
    // 获取当前页面索引
    size_t current_page = book->getCurrentPageIndex();
    
    // 初始化字体缓存（会自动缓存前2页、前1页、当前页、后1页、后2页）
    if (!g_font_buffer_manager.initialize(book, current_page)) {
        Serial.println("Failed to initialize font buffer manager");
        return;
    }
    
    Serial.printf("Font cache initialized for page %u\n", (unsigned)current_page);
    
    // 检查当前页缓存是否有效
    if (g_font_buffer_manager.isCacheValid(0)) {
        Serial.println("Current page cache is valid");
    }
    
    // 检查某个字符是否在当前页缓存中
    uint16_t test_char = 0x4E2D;  // '中' 字的Unicode
    if (g_font_buffer_manager.hasChar(test_char, 0)) {
        Serial.printf("Character U+%04X found in current page cache\n", test_char);
        
        // 获取字符的字形信息
        const CharGlyphInfo* info = g_font_buffer_manager.getCharGlyphInfo(test_char, 0);
        if (info) {
            Serial.printf("  Width: %u, BitmapW: %u, BitmapH: %u, Size: %u bytes\n",
                         info->width, info->bitmapW, info->bitmapH, info->bitmap_size);
            
            // 获取字符的位图数据
            const uint8_t* bitmap = g_font_buffer_manager.getCharBitmap(test_char, 0);
            if (bitmap) {
                Serial.printf("  Bitmap data available at address %p\n", bitmap);
            }
        }
    }
}

// ========== 翻页时更新缓存示例 ==========

void example_page_navigation()
{
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        return;
    }
    
    size_t current_page = book->getCurrentPageIndex();
    
    // 确保缓存已初始化
    if (!g_font_buffer_manager.isInitialized()) {
        g_font_buffer_manager.initialize(book, current_page);
    }
    
    // 翻到下一页
    if (book->hasNextPage()) {
        book->jumpToPage(current_page + 1);
        
        // 滚动更新缓存（向前翻页）
        g_font_buffer_manager.scrollUpdate(book, current_page + 1, true);
        
        Serial.printf("Moved to next page: %u\n", (unsigned)(current_page + 1));
    }
    
    // 翻到上一页
    if (book->hasPrevPage()) {
        book->jumpToPage(current_page - 1);
        
        // 滚动更新缓存（向后翻页）
        g_font_buffer_manager.scrollUpdate(book, current_page - 1, false);
        
        Serial.printf("Moved to previous page: %u\n", (unsigned)(current_page - 1));
    }
}

// ========== 查询相邻页面缓存示例 ==========

void example_adjacent_pages()
{
    uint16_t test_char = 0x6587;  // '文' 字的Unicode
    
    // 检查字符是否在前一页缓存中
    if (g_font_buffer_manager.hasChar(test_char, -1)) {
        Serial.printf("Character U+%04X found in previous page cache\n", test_char);
    }
    
    // 检查字符是否在后一页缓存中
    if (g_font_buffer_manager.hasChar(test_char, +1)) {
        Serial.printf("Character U+%04X found in next page cache\n", test_char);
    }
    
    // 检查字符是否在前两页缓存中
    if (g_font_buffer_manager.hasChar(test_char, -2)) {
        Serial.printf("Character U+%04X found in page-2 cache\n", test_char);
    }
    
    // 检查字符是否在后两页缓存中
    if (g_font_buffer_manager.hasChar(test_char, +2)) {
        Serial.printf("Character U+%04X found in page+2 cache\n", test_char);
    }
}

// ========== 批量查询示例 ==========

void example_batch_query()
{
    // 测试一组字符
    uint16_t test_chars[] = {
        0x4E2D,  // 中
        0x6587,  // 文
        0x7F16,  // 编
        0x7801,  // 码
        0x6D4B,  // 测
        0x8BD5,  // 试
    };
    
    int cache_counts[5] = {0, 0, 0, 0, 0};  // -2, -1, 0, +1, +2
    
    for (size_t i = 0; i < sizeof(test_chars) / sizeof(test_chars[0]); i++) {
        uint16_t unicode = test_chars[i];
        
        // 检查字符在各个缓存中的存在情况
        for (int offset = -2; offset <= 2; offset++) {
            if (g_font_buffer_manager.hasChar(unicode, offset)) {
                cache_counts[offset + 2]++;
            }
        }
    }
    
    // 打印统计结果
    Serial.println("Cache hit statistics:");
    const char* labels[] = {"Page-2", "Page-1", "Current", "Page+1", "Page+2"};
    for (int i = 0; i < 5; i++) {
        Serial.printf("  %s: %d/%d\n", labels[i], cache_counts[i], 
                     (int)(sizeof(test_chars) / sizeof(test_chars[0])));
    }
}

// ========== 性能测试示例 ==========

void example_performance_test()
{
    BookHandle* book = g_current_book;
    if (!book || !book->isOpen()) {
        return;
    }
    
    size_t current_page = book->getCurrentPageIndex();
    
    // 测试初始化时间
    unsigned long start_time = millis();
    g_font_buffer_manager.initialize(book, current_page);
    unsigned long init_time = millis() - start_time;
    
    Serial.printf("Cache initialization took %lu ms\n", init_time);
    
    // 测试查询性能
    uint16_t test_char = 0x4E2D;
    start_time = micros();
    for (int i = 0; i < 1000; i++) {
        g_font_buffer_manager.hasChar(test_char, 0);
    }
    unsigned long query_time = micros() - start_time;
    
    Serial.printf("1000 cache queries took %lu us (avg %.2f us)\n", 
                 query_time, query_time / 1000.0);
    
    // 测试滚动更新时间
    if (book->hasNextPage()) {
        start_time = millis();
        g_font_buffer_manager.scrollUpdate(book, current_page + 1, true);
        unsigned long scroll_time = millis() - start_time;
        
        Serial.printf("Scroll update took %lu ms\n", scroll_time);
    }
}

// ========== 清理缓存示例 ==========

void example_cleanup()
{
    // 清理所有缓存，释放内存
    g_font_buffer_manager.clearAll();
    
    Serial.println("All font caches cleared");
}

// ========== 集成到渲染流程的建议 ==========

/**
 * 建议的集成方式：
 * 
 * 1. 在 BookHandle::open() 或首次渲染时调用：
 *    g_font_buffer_manager.initialize(this, getCurrentPageIndex());
 * 
 * 2. 在 BookHandle::nextPage() 和 prevPage() 中调用：
 *    g_font_buffer_manager.scrollUpdate(this, new_page_index, forward);
 * 
 * 3. 在字体渲染函数（如 bin_font_print）中优先从缓存查询：
 *    const uint8_t* cached_bitmap = g_font_buffer_manager.getCharBitmap(unicode, 0);
 *    if (cached_bitmap) {
 *        // 使用缓存的位图数据
 *    } else {
 *        // 回退到原有的SD卡读取方式
 *    }
 * 
 * 4. 在 BookHandle::close() 或切换书籍时调用：
 *    g_font_buffer_manager.clearAll();
 * 
 * 5. 内存不足时可以选择性清理：
 *    // 只清理边缘页面的缓存
 *    g_font_buffer_manager.caches_[0].clear();  // 清理前2页
 *    g_font_buffer_manager.caches_[4].clear();  // 清理后2页
 */

