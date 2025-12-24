#pragma once

#include <Arduino.h>
#include <SD.h>
#include "readpaper.h"

// 前向声明
class BookHandle;

/**
 * 配置管理模块
 * 负责全局配置的保存、加载和持久化
 */

// 配置文件路径 - 存储在SD卡根目录
// 使用双文件交替策略以提高硬件复位时的鲁棒性
#define CONFIG_FILE_PATH "/readpaper.cfg"  // 旧版兼容路径（已弃用）
#define CONFIG_FILE_A "/readpaper.cfg.A"
#define CONFIG_FILE_B "/readpaper.cfg.B"

// 配置版本，用于兼容性检查
#define CONFIG_VERSION 1

/**
 * 初始化配置系统
 * 在系统启动时调用，会自动加载保存的配置
 * @return true 成功，false 失败
 */
bool config_init();

/**
 * 保存全局配置到文件
 * @return true 成功，false 失败
 */
bool config_save();

/**
 * 从文件加载全局配置
 * @return true 成功，false 失败或文件不存在
 */
bool config_load();

/**
 * 重置配置为默认值
 */
void config_reset_to_defaults();

/**
 * 检查配置文件是否存在
 * @return true 存在，false 不存在
 */
bool config_file_exists();

/**
 * 删除配置文件
 * @return true 成功，false 失败
 */
bool config_delete();

/**
 * 获取配置文件信息
 * @param file_size 输出文件大小
 * @param last_modified 输出最后修改时间（毫秒时间戳）
 * @return true 成功，false 失败
 */
bool config_get_file_info(size_t* file_size, unsigned long* last_modified);

// 调试和统计信息
struct ConfigStats {
    unsigned long total_saves = 0;      // 总保存次数
    unsigned long total_loads = 0;      // 总加载次数
    unsigned long last_save_time = 0;   // 最后保存时间
    unsigned long last_load_time = 0;   // 最后加载时间
    uint32_t sequence = 0;              // 配置序列号（用于双文件交替）
};

/**
 * 获取配置统计信息
 * @return 配置统计结构
 */
ConfigStats config_get_stats();

/**
 * 设置当前阅读文件路径并保存配置
 * @param file_path 文件路径（支持/sd/或/spiffs/前缀）
 * @return true 成功，false 失败
 */
bool config_set_current_file(const char* file_path);

/**
 * 更新当前阅读文件并创建BookHandle实例
 * 如果文件不存在或打开失败，会自动回退到默认文件
 * @param file_path 文件路径（支持/sd/或/spiffs/前缀）
 * @param area_w 显示区域宽度
 * @param area_h 显示区域高度  
 * @param fsize 字体大小
 * @return BookHandle指针，失败返回nullptr
 */
BookHandle* config_update_current_book(const char* file_path, int16_t area_w, int16_t area_h, float fsize);