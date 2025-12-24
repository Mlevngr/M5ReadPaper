#include "test/test_functions.h"
#include "test/per_file_debug.h"
#include <SD.h>
#include "../SD/SDWrapper.h"
#include "device/file_manager.h"
#include "device/file_reader.h"
#include "device/ui_display.h"
#include "text/bin_font_print.h"
#include "device/efficient_file_scanner.h"
#include "text/text_handle.h"
#include "text/book_handle.h"
#include "current_book.h"
#include "config/config_manager.h"

#include "../globals.h"

extern GlobalConfig g_config;

namespace test
{

    void canvas_demo_quick_test()
    {
        // minimal placeholder for quick visually testing canvas
        bin_font_clear_canvas();
        bin_font_print("Canvas Quick Test", 30, 0, 10, 10, 200, false); // 1.0f * 30 = 30
        bin_font_flush_canvas();
    }

    void canvas_demo_init() { /* placeholder */ }
    void canvas_demo_welcome_screen() { /* placeholder */ }
    void canvas_demo_button_test() { /* placeholder */ }
    void canvas_demo_text_display() { /* placeholder */ }
    void canvas_demo_file_list() { /* placeholder */ }
    void canvas_demo_reading_interface() { /* placeholder */ }
    void canvas_demo_settings_panel() { /* placeholder */ }

    void scan_sd_book_directory()
    {
#if DBG_TEST_FUNCTIONS
        Serial.println("[SCAN] Scanning /sd/book directory...");
#endif
        File root = SDW::SD.open("/book");
        if (!root)
        {
#if DBG_TEST_FUNCTIONS
            Serial.println("[ERROR] Failed to open /sd/book directory");
#endif
            return;
        }

        if (!root.isDirectory())
        {
            Serial.println("[ERROR] /sd/book is not a directory");
            return;
        }

        // 使用高效文件扫描器
        std::vector<FileInfo> bookFiles = EfficientFileScanner::scanDirectory("/book");
        for (const auto& fileInfo : bookFiles) {
            if (!fileInfo.isDirectory) {
#if DBG_TEST_FUNCTIONS
                Serial.printf("[FILE] %s (%d bytes)\n", fileInfo.name.c_str(), (int)fileInfo.size);
#endif
            }
        }
        
        root.close();
    }

    void print_sample_pages()
    {
        // 自动翻页：先向后翻10页，每页间隔1秒；然后再向回翻10页
        const char *file_path = "/sd/book/1971-欢乐英雄.txt";
        int16_t area_w = PAPER_S3_WIDTH - MARGIN_LEFT - MARGIN_RIGHT;
        int16_t area_h = PAPER_S3_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM;
        float fsize = (float)get_font_size_from_file(); // 使用字体文件中的实际大小

#if DBG_TEST_FUNCTIONS
        Serial.printf("[TEST] auto-paging start file=%s\n", file_path);
#endif

        // 创建并发布 BookHandle 实例（使用 shared_ptr）
        {
            std::shared_ptr<BookHandle> new_book = std::make_shared<BookHandle>(std::string(file_path), area_w, area_h, fsize, TextEncoding::AUTO_DETECT);
            std::atomic_store(&__g_current_book_shared, new_book);
            // reset autoread when test creates a new book
            autoread = false;
        }

        
        // 移除强制设置位置为0，让书签系统自然工作
        // g_current_book->setPosition(start_pos);  // 注释掉这行，让构造函数中的书签加载生效
        
        // 注意：这里不删除 g_current_book，因为它可能在其他地方（如状态机）被使用
        // 如果需要清理，应该在适当的地方（如程序结束或切换书籍时）调用
        // delete g_current_book; g_current_book = nullptr;
    }

    void test_config_manager()
    {
        Serial.println("[CONFIG_TEST] 开始配置管理器测试");

        // 获取当前配置状态
        ConfigStats stats = config_get_stats();
        Serial.printf("[CONFIG_TEST] 初始统计 - 保存次数: %lu, 加载次数: %lu\n", 
                     stats.total_saves, stats.total_loads);

        // 测试配置文件是否存在
        bool file_exists = config_file_exists();
        Serial.printf("[CONFIG_TEST] 配置文件存在: %s\n", file_exists ? "是" : "否");

        if (file_exists) {
            size_t file_size = 0;
            unsigned long last_modified = 0;
            if (config_get_file_info(&file_size, &last_modified)) {
                Serial.printf("[CONFIG_TEST] 配置文件大小: %zu 字节, 最后修改: %lu\n", 
                             file_size, last_modified);
            }
        }

        // 保存当前配置
        uint8_t original_rotation = g_config.rotation;
        char original_file[256];
        strcpy(original_file, g_config.currentReadFile);
        
        Serial.printf("[CONFIG_TEST] 原始旋转值: %d\n", original_rotation);
        Serial.printf("[CONFIG_TEST] 原始文件路径: %s\n", original_file);

        // 测试设置当前文件功能
        Serial.println("[CONFIG_TEST] 测试设置当前文件功能");
        if (config_set_current_file("/sd/book/test.txt")) {
            Serial.printf("[CONFIG_TEST] 成功设置当前文件: %s\n", g_config.currentReadFile);
        } else {
            Serial.println("[CONFIG_TEST] 设置当前文件失败");
        }

        // 修改配置并保存
        g_config.rotation = (original_rotation == 0) ? 2 : 0;
        
        Serial.printf("[CONFIG_TEST] 修改后旋转值: %d\n", g_config.rotation);
        Serial.printf("[CONFIG_TEST] 修改后文件路径: %s\n", g_config.currentReadFile);

        // 等待一段时间模拟操作
        delay(100);

        // 重新加载配置验证
        if (config_load()) {
            Serial.printf("[CONFIG_TEST] 重新加载后旋转值: %d\n", g_config.rotation);
            Serial.printf("[CONFIG_TEST] 重新加载后文件路径: %s\n", g_config.currentReadFile);
        } else {
            Serial.println("[CONFIG_TEST] 重新加载失败");
        }

        // 恢复原始配置
        g_config.rotation = original_rotation;
        strcpy(g_config.currentReadFile, original_file);
        config_save();

        // 获取最终统计
        stats = config_get_stats();
        Serial.printf("[CONFIG_TEST] 最终统计 - 保存次数: %lu, 加载次数: %lu\n", 
                     stats.total_saves, stats.total_loads);

        Serial.println("[CONFIG_TEST] 配置管理器测试完成");
    }

} // namespace test
