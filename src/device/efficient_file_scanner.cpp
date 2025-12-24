#include "efficient_file_scanner.h"
#include "globals.h"
#include "readpaper.h"

std::vector<FileInfo> EfficientFileScanner::scanDirectory(const std::string& dirPath, const std::string& extension) {
    std::vector<FileInfo> results;
    if (g_disable_sd_access) return results;
    
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 扫描目录: %s, 扩展名: %s\n", dirPath.c_str(), extension.c_str());
    unsigned long startTime = millis();
#endif
    
    File dir = SDW::SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
#if DBG_FILE_MANAGER
        Serial.printf("[EFS] 无法打开目录: %s\n", dirPath.c_str());
#endif
        return results;
    }
    
    scanDirectoryInternal(dir, dirPath, results, extension);
    dir.close();
    
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 扫描完成，找到 %d 个文件，耗时: %lu ms\n", 
                 (int)results.size(), millis() - startTime);
#endif
    
    return results;
}

int EfficientFileScanner::countFiles(const std::string& dirPath, const std::string& extension) {
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 计数文件: %s, 扩展名: %s\n", dirPath.c_str(), extension.c_str());
    unsigned long startTime = millis();
#endif
    
    if (g_disable_sd_access) return 0;
    File dir = SDW::SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
#if DBG_FILE_MANAGER
        Serial.printf("[EFS] 无法打开目录: %s\n", dirPath.c_str());
#endif
        return 0;
    }
    
    int count = 0;
    dir.rewindDirectory();
    
    // 使用轻量级方法遍历目录条目
    while (true) {
        // 检查内存状态
        if (ESP.getFreeHeap() < 4096) {
#if DBG_FILE_MANAGER
            Serial.printf("[EFS] 内存不足，停止计数\n");
#endif
            break;
        }
        
        File entry = dir.openNextFile();
        if (!entry) break;
        
        // 只获取基本信息，不读取文件内容，添加安全检查
        const char* namePtr = entry.name();
        if (!namePtr || strlen(namePtr) == 0) {
            entry.close();
            continue;
        }
        
        std::string fileName = std::string(namePtr);
        if (fileName.length() > 255) { // 支持最长 255 字符的文件名（LFN 上限）
            fileName = fileName.substr(0, 255);
        }
        
        bool isDir = entry.isDirectory();
        
        // 立即关闭文件句柄
        entry.close();
        
        // 如果是文件且符合扩展名要求
        if (!isDir) {
            if (extension.empty() || hasExtension(fileName, extension)) {
                count++;
            }
        }
        
        // 防止过多文件导致系统负载
        if (count > 500) {
#if DBG_FILE_MANAGER
            Serial.printf("[EFS] 文件数量过多，停止计数\n");
#endif
            break;
        }
        
        // 每10个文件让出CPU控制权，防止watchdog超时
        if (count % 10 == 0) {
            yield();
            delayMicroseconds(50);
        }
    }
    
    dir.close();
    
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 计数完成，找到 %d 个文件，耗时: %lu ms\n", 
                 count, millis() - startTime);
#endif
    
    return count;
}

std::vector<FileInfo> EfficientFileScanner::scanDirectoryPaged(const std::string& dirPath, 
                                                              int page, int perPage, 
                                                              const std::string& extension) {
    std::vector<FileInfo> results;
    
    if (page < 1 || perPage < 1) return results;
    
    int startIndex = (page - 1) * perPage;
    
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 分页扫描: %s, 页码: %d, 每页: %d, 开始索引: %d\n", 
                 dirPath.c_str(), page, perPage, startIndex);
    unsigned long startTime = millis();
#endif
    
    if (g_disable_sd_access) return results;
    File dir = SDW::SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
#if DBG_FILE_MANAGER
        Serial.printf("[EFS] 无法打开目录: %s\n", dirPath.c_str());
#endif
        return results;
    }
    
    scanDirectoryInternal(dir, dirPath, results, extension, startIndex, perPage);
    dir.close();
    
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 分页扫描完成，返回 %d 个文件，耗时: %lu ms\n", 
                 (int)results.size(), millis() - startTime);
#endif
    
    return results;
}

bool EfficientFileScanner::fileExists(const std::string& filePath) {
    // 使用SD.exists()方法，不需要打开文件
    if (g_disable_sd_access) return false;
    return SDW::SD.exists(filePath.c_str());
}

size_t EfficientFileScanner::getFileSize(const std::string& filePath) {
    if (g_disable_sd_access) return 0;
    File file = SDW::SD.open(filePath.c_str());
    if (!file) return 0;

    size_t size = file.size();
    file.close();
    return size;
}

bool EfficientFileScanner::hasExtension(const std::string& filename, const std::string& extension) {
    if (extension.empty()) return true;
    
    if (filename.length() < extension.length()) return false;
    
    // 转换为小写比较
    std::string fileExt = filename.substr(filename.length() - extension.length());
    std::string targetExt = extension;
    
    // 简单的小写转换
    for (char& c : fileExt) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
    for (char& c : targetExt) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
    
    return fileExt == targetExt;
}

std::string EfficientFileScanner::getFileName(const std::string& fullPath) {
    size_t pos = fullPath.find_last_of("/\\");
    if (pos == std::string::npos) return fullPath;
    return fullPath.substr(pos + 1);
}

void EfficientFileScanner::scanDirectoryInternal(File& dir, const std::string& basePath,
                                                std::vector<FileInfo>& results, 
                                                const std::string& extension,
                                                int startIndex, int maxCount) {
    dir.rewindDirectory();
    
    int currentIndex = 0;
    int addedCount = 0;
    
    while (true) {
        // 内存检查
        if (ESP.getFreeHeap() < 4096) {
#if DBG_FILE_MANAGER
            Serial.printf("[EFS] 内存不足 (%d bytes)，停止扫描\n", ESP.getFreeHeap());
#endif
            break;
        }
        
        File entry = dir.openNextFile();
        if (!entry) break;
        
        // 获取基本信息，添加安全检查
        const char* namePtr = entry.name();
        if (!namePtr || strlen(namePtr) == 0) {
            entry.close();
            continue;
        }
        
        std::string fileName = std::string(namePtr);
        if (fileName.length() > 255) { // 支持最长 255 字符的文件名（LFN 上限）
            fileName = fileName.substr(0, 255);
        }
        
        std::string fullPath = basePath + "/" + fileName;
        size_t fileSize = entry.isDirectory() ? 0 : entry.size();
        bool isDir = entry.isDirectory();
        
        // 立即关闭文件句柄，释放资源
        entry.close();
        
        // 检查是否符合条件
        bool shouldInclude = false;
        if (isDir) {
            shouldInclude = extension.empty(); // 只有在没有扩展名限制时才包含目录
        } else {
            shouldInclude = extension.empty() || hasExtension(fileName, extension);
        }
        
        if (shouldInclude) {
            // 如果在分页模式下，检查索引范围
            if (maxCount > 0) {
                if (currentIndex >= startIndex) {
                    results.emplace_back(fileName, fullPath, fileSize, isDir);
                    addedCount++;
                    
                    // 达到页面大小限制
                    if (addedCount >= maxCount) break;
                }
                currentIndex++;
            } else {
                // 非分页模式，直接添加
                results.emplace_back(fileName, fullPath, fileSize, isDir);
            }
        }
        
    // 安全限制，防止处理过多文件
    if (results.size() >= MAX_MAIN_MENU_FILE_COUNT) {
#if DBG_FILE_MANAGER
        Serial.printf("[EFS] 文件数量过多，停止扫描\n");
#endif
        break;
    }
        
        // 每处理5个文件让出CPU控制权，防止watchdog超时
        if ((currentIndex + addedCount) % 5 == 0) {
            yield(); // 让出CPU控制权
            delayMicroseconds(100); // 短暂延迟
        }
    }
}

std::vector<FileInfo> EfficientFileScanner::scanSPIFFSDirectory(const std::string& dirPath, const std::string& extension) {
    std::vector<FileInfo> results;
#if DBG_FILE_MANAGER
    Serial.printf("[EFS] 扫描 SPIFFS 目录: %s, 扩展名: %s\n", dirPath.c_str(), extension.c_str());
    unsigned long startTime = millis();
#endif

    File dir = SPIFFS.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
#if DBG_FILE_MANAGER
        Serial.printf("[EFS] 无法打开 SPIFFS 目录: %s\n", dirPath.c_str());
#endif
        return results;
    }

    scanDirectoryInternal(dir, dirPath, results, extension);
    dir.close();

#if DBG_FILE_MANAGER
    Serial.printf("[EFS] SPIFFS 扫描完成，找到 %d 个文件，耗时: %lu ms\n", (int)results.size(), millis() - startTime);
#endif

    return results;
}