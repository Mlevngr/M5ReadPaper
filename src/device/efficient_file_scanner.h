#pragma once

#include "SD/SDWrapper.h"
#include <SPIFFS.h>
#include <vector>
#include <string>

// 文件信息结构体 - 只包含基本信息，无需打开文件
struct FileInfo {
    std::string name;
    std::string path;
    size_t size;
    bool isDirectory;
    
    FileInfo() : size(0), isDirectory(false) {}
    FileInfo(const std::string& n, const std::string& p, size_t s, bool dir) 
        : name(n), path(p), size(s), isDirectory(dir) {}
};

// 高效文件扫描器类
class EfficientFileScanner {
public:
    // 扫描指定目录，返回文件信息列表
    static std::vector<FileInfo> scanDirectory(const std::string& dirPath, const std::string& extension = "");

    // Scan SPIFFS directory (similar to scanDirectory but uses SPIFFS)
    static std::vector<FileInfo> scanSPIFFSDirectory(const std::string& dirPath, const std::string& extension = "");
    
    // 扫描并计数，不返回详细信息（最高效）
    static int countFiles(const std::string& dirPath, const std::string& extension = "");
    
    // 分页扫描 - 只返回指定页的文件信息
    static std::vector<FileInfo> scanDirectoryPaged(const std::string& dirPath, 
                                                   int page, int perPage, 
                                                   const std::string& extension = "");
    
    // 检查文件是否存在（不打开文件）
    static bool fileExists(const std::string& filePath);
    
    // 获取文件大小（不读取文件内容）
    static size_t getFileSize(const std::string& filePath);
    
private:
    // 检查文件扩展名
    static bool hasExtension(const std::string& filename, const std::string& extension);
    
    // 从完整路径获取文件名
    static std::string getFileName(const std::string& fullPath);
    
    // 内部高效扫描函数
    static void scanDirectoryInternal(File& dir, const std::string& basePath,
                                    std::vector<FileInfo>& results, 
                                    const std::string& extension,
                                    int startIndex = 0, int maxCount = -1);
};