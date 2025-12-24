#pragma once
// 字体缓冲区管理 - 支持5页滑动窗口 + 通用字符缓存

#include <cstdint>
#include <cstddef>
#include <vector>
#include "bin_font_print.h" // 提供 PSRAMAllocator
#include <unordered_set>
#include <string>

class BookHandle;

// 页面字体缓存系统常量
constexpr size_t FONT_CACHE_PAGE_COUNT = 5;  // 缓存5个页面：前2页、前1页、当前页、后1页、后2页
constexpr size_t FONT_CACHE_CENTER_INDEX = 2; // 中心位置（当前页）在数组中的索引

// 字符字形索引信息
struct CharGlyphInfo {
    uint16_t unicode;         // 字符的Unicode编码
    uint16_t width;           // 字符宽度
    uint8_t bitmapW;          // 位图宽度
    uint8_t bitmapH;          // 位图高度
    int8_t x_offset;          // X偏移
    int8_t y_offset;          // Y偏移
    uint32_t bitmap_size;     // 字形位图的字节数
    uint32_t bitmap_offset;   // 字形位图在本缓冲区中的偏移量
} __attribute__((packed));

// 页面字体缓存池头部信息
struct PageFontCacheHeader {
    uint32_t total_size;      // 缓冲区总大小（包括头部、索引区、位图区）
    uint32_t char_count;      // 包含的字符个数
    uint32_t index_offset;    // 字符索引区的起始偏移（紧跟在头部之后）
    uint32_t bitmap_offset;   // 位图数据区的起始偏移
} __attribute__((packed));

// 页面缓存构建统计
struct PageFontCacheStats {
    uint32_t build_ms = 0;           // 构建耗时
    uint32_t reused_from_cache = 0;  // 从其他页面缓存复用的字形数
    uint32_t loaded_from_sd = 0;     // 从SD读取的字形数
    uint32_t total_chars = 0;        // 总字符数
    uint32_t unique_chars = 0;       // 去重后字符数
};

// 单个页面的字体缓存池
class PageFontCache {
    // FontBufferManager 需要访问私有成员进行缓存交换
    friend class FontBufferManager;
    
public:
    PageFontCache();
    ~PageFontCache();

    // 禁用拷贝和赋值
    PageFontCache(const PageFontCache&) = delete;
    PageFontCache& operator=(const PageFontCache&) = delete;

    // 构建页面字体缓存
    bool build(BookHandle* book, size_t page_index);
    
    // 清理缓存
    void clear();
    
    // 查询字符是否在缓存中
    bool hasChar(uint16_t unicode) const;
    
    // 获取字符的字形信息（返回nullptr表示未找到）
    const CharGlyphInfo* getCharGlyphInfo(uint16_t unicode) const;
    
    // 按索引获取字形信息（用于遍历缓存中的所有字符）
    const CharGlyphInfo* getCharGlyphInfoByIndex(size_t index) const;
    
    // 获取字符的位图数据（返回nullptr表示未找到）
    const uint8_t* getCharBitmap(uint16_t unicode) const;
    
    // 获取缓存状态
    bool isValid() const { return buffer_ != nullptr; }
    size_t getCharCount() const;
    size_t getTotalSize() const;
    
    // 交换两个缓存的内容（用于滚动更新）
    void swapWith(PageFontCache& other);
    
    // 设置缓存内容（用于通用缓存构建）
    void setCache(uint8_t* buffer, PageFontCacheHeader* header,
                  CharGlyphInfo* index_area, uint8_t* bitmap_area,
                  uint32_t build_ms, uint32_t loaded_from_sd,
                  uint32_t unique_chars, uint32_t total_chars);
    
private:
    uint8_t* buffer_;                                 // 缓冲区指针
    PageFontCacheHeader* header_;                     // 头部指针
    CharGlyphInfo* index_area_;                       // 字符索引区指针
    uint8_t* bitmap_area_;                            // 位图数据区指针
    
    // 辅助函数：从页面文本中提取唯一字符（使用 PSRAMAllocator 以降低内部 DRAM 压力）
    std::vector<uint16_t, PSRAMAllocator<uint16_t>> extractUniqueChars(const std::string& page_text);

    // 辅助函数：计算所需缓冲区大小（chars/glyph_infos 使用 PSRAM 分配器）
    size_t calculateBufferSize(const std::vector<uint16_t, PSRAMAllocator<uint16_t>>& chars,
                               std::vector<CharGlyphInfo, PSRAMAllocator<CharGlyphInfo>>& glyph_infos);

    PageFontCacheStats stats_{};
};

// 页面字体缓存管理器
class FontBufferManager {
public:
    FontBufferManager();
    ~FontBufferManager();

    // 禁用拷贝和赋值
    FontBufferManager(const FontBufferManager&) = delete;
    FontBufferManager& operator=(const FontBufferManager&) = delete;

    // 初始化/更新缓存：基于当前页面索引，构建5个页面的字体缓存
    // current_page_index: 当前页面索引
    bool initialize(BookHandle* book, size_t current_page_index);
    
    // 清理所有缓存
    void clearAll();
    
    // 滚动更新缓存：当翻页时，重用已有缓存，只构建新页面
    // new_current_page: 新的当前页面索引
    // forward: true表示向前翻页，false表示向后翻页
    bool scrollUpdate(BookHandle* book, size_t new_current_page, bool forward);
    
    // 临时禁用/启用缓存初始化（用于构建缓存时避免递归更新）
    void setInitializationLocked(bool locked) { initialization_locked_ = locked; }
    bool isInitializationLocked() const { return initialization_locked_; }
    
    // 查询字符是否在当前页或附近页面的缓存中
    bool hasChar(uint16_t unicode, int page_offset = 0) const;
    
    // 从指定页面偏移的缓存中获取字符字形信息
    // page_offset: 相对于当前页的偏移（-2到+2）
    const CharGlyphInfo* getCharGlyphInfo(uint16_t unicode, int page_offset = 0) const;
    
    // 从指定页面偏移的缓存中获取字符位图
    const uint8_t* getCharBitmap(uint16_t unicode, int page_offset = 0) const;

    // 从任何已构建的缓存中查找位图（用于构建新缓存时复用，包括通用缓存）
    const uint8_t* getCharBitmapAny(uint16_t unicode) const;

    // 在不影响当前渲染的情况下补全周边缓存（仅构建缺失的 ±1/±2）
    void prefetchAround(BookHandle* book);
    
    // 获取管理器状态
    bool isInitialized() const { return initialized_; }
    size_t getCurrentPageIndex() const { return current_page_index_; }
    
    // 获取指定偏移的缓存状态
    bool isCacheValid(int page_offset) const;

    // 统计缓存命中情况
    struct CacheStats {
        uint32_t hits;
        uint32_t misses;
    };
    CacheStats getStats() const { return stats_; }
    void resetStats();
    void logStats() const;
    // 运行时日志开关（默认由编译期宏 DBG_FONT_BUFFER 控制）
    void setLogEnabled(bool enabled);
    bool isLogEnabled() const;
    
private:
    PageFontCache caches_[FONT_CACHE_PAGE_COUNT];  // 5个页面的缓存池
    size_t current_page_index_;                     // 当前页面索引
    bool initialized_;                              // 是否已初始化
    bool initialization_locked_;                    // 是否锁定初始化（构建缓存时使用）
    mutable CacheStats stats_;
    
        bool log_enabled_ = 
    #if defined(DBG_FONT_BUFFER)
        true;
    #else
        false;
    #endif
    // 获取缓存数组索引
    int getCacheIndex(int page_offset) const;
    
    // 验证页面偏移是否有效
    bool isValidPageOffset(int page_offset) const;
};

// 全局字体缓存管理器实例（定义在font_buffer.cpp中）
extern FontBufferManager g_font_buffer_manager;

// 通用字符缓存（UI和常用字符）
extern PageFontCache g_common_char_cache;

// 书籍文件名字体缓存（用于文件列表显示）
extern PageFontCache g_bookname_char_cache;

// TOC（目录）专用字体缓存（用于书籍目录显示）
extern PageFontCache g_toc_char_cache;

// 通用回收池缓存（回收其他缓存释放的字符，上限1000字符）
extern PageFontCache g_common_recycle_pool;

// 构建通用字符缓存（在加载字体时调用）
void buildCommonCharCache();

// 初始化通用回收池（字体加载时调用，创建空池）
void initCommonRecyclePool();

// 从即将释放的缓存中回收字符到回收池（页面缓存释放前调用）
// cache: 即将被释放的缓存对象
void recycleCharsToPool(const PageFontCache* cache);

// 清空通用回收池（字体卸载时调用）
void clearCommonRecyclePool();

// 获取通用字符列表
std::string getCommonCharList();

// 添加书籍文件名字符到缓存（增量更新，最多300字）
void addBookNamesToCache(const std::vector<std::string>& bookNames);

// 清空书籍文件名缓存
void clearBookNameCache();

// 构建TOC字符缓存（在打开/切换书籍时调用，如果书籍有TOC）
// toc_file_path: TOC文件路径（例如 "/sd/book.idx"）
void buildTocCharCache(const char* toc_file_path);

// 清空TOC字符缓存
void clearTocCache();
