#include "book_file_manager.h"
#include "globals.h"
#include "text/font_buffer.h"
#include <algorithm>
#include <cctype>
#include "readpaper.h"

extern GlobalConfig g_config;

// 静态成员初始化
std::vector<std::string> BookFileManager::cachedBookNames;
bool BookFileManager::cacheValid = false;
unsigned long BookFileManager::lastScanTime = 0;

int BookFileManager::getBookCount() {
    if (shouldRefreshCache()) {
        scanBooks();
    }
    return (int)cachedBookNames.size();
}

std::vector<std::string> BookFileManager::getBookList(int page, int perPage) {
    if (shouldRefreshCache()) {
        scanBooks();
    }
    
    std::vector<std::string> result;
    if (page < 1 || perPage < 1) return result;
    
    int startIndex = (page - 1) * perPage;
    int endIndex = startIndex + perPage;
    
    for (int i = startIndex; i < endIndex && i < (int)cachedBookNames.size(); i++) {
        result.push_back(cachedBookNames[i]);
    }
    
    return result;
}

std::vector<std::string> BookFileManager::getAllBookNames() {
    if (shouldRefreshCache()) {
        scanBooks();
    }
    return cachedBookNames;
}

void BookFileManager::refreshCache() {
    cacheValid = false;
    scanBooks();
}

bool BookFileManager::bookExists(const std::string& bookName) {
    // 构建完整文件路径
    std::string fullPath = "/book/" + bookName + ".txt";
    return EfficientFileScanner::fileExists(fullPath);
}

size_t BookFileManager::getBookSize(const std::string& bookName) {
    std::string fullPath = "/book/" + bookName + ".txt";
    return EfficientFileScanner::getFileSize(fullPath);
}

void BookFileManager::clearCache() {
    cachedBookNames.clear();
    cacheValid = false;
    lastScanTime = 0;
}

void BookFileManager::scanBooks() {
    if (cacheValid) return;
    
    // 检查内存状态
    if (ESP.getFreeHeap() < 8192) {
#if DBG_FILE_MANAGER
        Serial.printf("[BookFileManager] 内存不足 (%d bytes)，跳过扫描\n", ESP.getFreeHeap());
#endif
        return;
    }
    
#if DBG_FILE_MANAGER
    Serial.printf("[BookFileManager] 开始扫描书籍文件，剩余内存: %d bytes\n", ESP.getFreeHeap());
    unsigned long startTime = millis();
#endif
    
    cachedBookNames.clear();
    
    // 使用高效文件扫描器扫描.txt文件
    std::vector<FileInfo> txtFiles = EfficientFileScanner::scanDirectory("/book", ".txt");
    
    // 检查扫描结果是否有效
    bool scanSuccess = true;
    
    for (const auto& fileInfo : txtFiles) {
        // 内存检查
        if (ESP.getFreeHeap() < 4096) {
#if DBG_FILE_MANAGER
            Serial.printf("[BookFileManager] 内存不足，停止处理文件\n");
#endif
            scanSuccess = false;
            break;
        }
        
        if (!fileInfo.isDirectory && !fileInfo.name.empty()) {
            // 移除.txt扩展名
            std::string bookName = removeExtension(fileInfo.name);
            if (!bookName.empty() && bookName.length() <= 255) { // 支持最长 255 字符的书名显示
                cachedBookNames.push_back(bookName);
            }
        }
        
        // 防止处理过多文件 - 限制最多 g_config.main_menu_file_count 个文件（受编译期上限保护）
        {
            size_t runtimeLimit = (size_t)g_config.main_menu_file_count;
            size_t cap = (size_t)MAX_MAIN_MENU_FILE_COUNT;
            size_t effective = runtimeLimit < cap ? runtimeLimit : cap;
            if (cachedBookNames.size() >= effective) {
#if DBG_FILE_MANAGER
                Serial.printf("[BookFileManager] 已达到%d个文件限制，停止处理\n", (int)effective);
#endif
                break;
            }
        }
    }
    
    // 只有在扫描成功且有合理结果时才标记缓存有效
    if (scanSuccess && ESP.getFreeHeap() > 4096) {
        cacheValid = true;
        lastScanTime = millis();
        
#if DBG_FILE_MANAGER
        Serial.printf("[BookFileManager] 扫描完成，找到 %d 本书，耗时: %lu ms，剩余内存: %d bytes\n", 
                     (int)cachedBookNames.size(), millis() - startTime, ESP.getFreeHeap());
#endif

        // 构建书籍文件名字体缓存
        if (!cachedBookNames.empty() && ESP.getFreeHeap() > 32768) {
            addBookNamesToCache(cachedBookNames);
        }
    } else {
#if DBG_FILE_MANAGER
        Serial.printf("[BookFileManager] 扫描失败或内存不足，清空缓存\n");
#endif
        cachedBookNames.clear();
        cacheValid = false;
    }

    // Ensure deterministic display order: sort cached book names alphabetically (case-insensitive)
    if (!cachedBookNames.empty()) {
        std::sort(cachedBookNames.begin(), cachedBookNames.end(), [](const std::string &a, const std::string &b) {
            size_t na = a.size();
            size_t nb = b.size();
            for (size_t i = 0; i < na && i < nb; ++i) {
                char ca = a[i]; char cb = b[i];
                if ((unsigned char)ca >= 'A' && (unsigned char)ca <= 'Z') ca = ca - 'A' + 'a';
                if ((unsigned char)cb >= 'A' && (unsigned char)cb <= 'Z') cb = cb - 'A' + 'a';
                if (ca < cb) return true;
                if (ca > cb) return false;
            }
            return na < nb;
        });
    }
}

bool BookFileManager::shouldRefreshCache() {
    // 如果缓存无效，需要刷新
    if (!cacheValid) return true;
    
    // 如果超过30秒，考虑刷新缓存
    if (millis() - lastScanTime > 30000) {
        cacheValid = false;
        return true;
    }
    
    // 缓存仍然有效
    return false;
}

std::string BookFileManager::removeExtension(const std::string& filename, const std::string& ext) {
    if (filename.length() > ext.length()) {
        size_t pos = filename.length() - ext.length();
        if (filename.substr(pos) == ext) {
            return filename.substr(0, pos);
        }
    }
    return filename;
}