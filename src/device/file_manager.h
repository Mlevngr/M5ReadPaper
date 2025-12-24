#pragma once
#include <M5Unified.h>
#include <vector>
#include "text/bin_font_print.h" // 提供 PSRAMAllocator
#include <string>

/**
 * @brief 字体文件信息结构体
 */
struct FontFileInfo {
    std::string path;          // 字体文件路径
    std::string family_name;   // 字体族名
    std::string style_name;    // 字体样式名
    uint8_t font_size;         // 字体大小
    uint8_t version;           // 字体版本
    size_t file_size;          // 文件大小（字节）
};

/**
 * @brief 初始化文件系统
 * @return true 初始化成功, false 初始化失败
 */
bool init_filesystem();

/**
 * @brief 列出SD卡根目录文件
 * @return 文件数量
 */
int list_root_files();

/**
 * @brief 显示SPIFFS文件系统中的文件
 */
void display_spiffs_files();

/**
 * @brief 扫描SD卡/font/目录下的bin格式字体文件
 * @return 包含字体信息的向量数组
 */
std::vector<FontFileInfo, PSRAMAllocator<FontFileInfo>> scan_font_files();

// 全局字体列表，由文件系统初始化或手动刷新时填充（存放于 PSRAM）
extern std::vector<FontFileInfo, PSRAMAllocator<FontFileInfo>> g_font_list;

// 扫描并刷新全局字体列表（会调用 scan_font_files()），可在 init_filesystem 后调用
void font_list_scan();

/**
 * @brief 将 g_font_list 中指定索引的字体移动到首位（索引基于当前 g_font_list）
 * @param ind 要移动到首位的索引，如果越界则无操作
 */
void update_font_list_order(int16_t ind);

/**
 * @brief 将带有伪前缀的路径（/sd/... 或 /spiffs/...）解析为真实路径并返回应使用的文件系统
 * @param fake_path 输入路径（可能以 /sd 或 /spiffs 开头）
 * @param out_real_path 输出真实路径（去掉伪前缀，仍以 '/' 开头）
 * @param out_use_spiffs 输出：true 表示应使用 SPIFFS，否则使用 SD
 * @return true 成功解析（out_real_path 被填充），false 表示输入为空
 */
bool resolve_fake_path(const std::string &fake_path, std::string &out_real_path, bool &out_use_spiffs);