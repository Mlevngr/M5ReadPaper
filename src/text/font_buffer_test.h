#pragma once

/**
 * 页面字体缓存系统测试函数声明
 * 
 * 这些函数用于验证和性能测试字体缓存系统
 * 可以在 debug 菜单或其他调试界面中调用
 */

// 基本功能测试
void test_font_buffer_basic();

// 翻页性能测试
void test_font_buffer_page_navigation();

// 缓存命中率测试
void test_font_buffer_hit_rate();

// 清理测试
void test_font_buffer_cleanup();

// 运行所有测试
void run_all_font_buffer_tests();

