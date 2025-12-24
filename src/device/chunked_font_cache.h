#pragma once
#include <M5Unified.h>
#include <FS.h>
#include <unordered_map>
#include <vector>

// 完整分块字体缓存 - 将整个字体文件分块存储在内存中
class ChunkedFontCache {
private:
    struct ChunkData {
        uint8_t* data;
        size_t size;             // 当前数据大小（压缩后）
        size_t original_size;    // 原始数据大小（压缩前）
        bool loaded;
        bool compressed_1bit;    // 是否为1bit压缩数据
    };
    
    std::vector<ChunkData> chunks;  // 所有块的信息（按chunk_id顺序）
    
    File* font_file;
    size_t total_font_size;        // 字体文件总大小
    size_t chunk_size;             // 单个块大小
    uint32_t total_chunks;         // 总块数
    bool fully_loaded;             // 是否完全加载成功
    
    // 统计信息
    uint32_t successful_chunks;    // 成功加载的块数
    uint32_t failed_chunks;        // 失败的块数
    size_t total_allocated;        // 总分配内存
    size_t compression_saved;      // 压缩节省的内存
    bool enable_1bit_compression;  // 是否启用1bit压缩
    
public:
    ChunkedFontCache();
    ~ChunkedFontCache();
    
    bool load_entire_font_chunked(File& fontFile, size_t chunk_kb = 256);
    bool read_data(uint32_t offset, uint8_t* dest, uint32_t size);
    void cleanup();
    void print_stats();
    bool is_fully_loaded() const { return fully_loaded; }
    
private:
    uint32_t get_chunk_id(uint32_t offset) { return offset / chunk_size; }
    uint32_t get_chunk_offset(uint32_t chunk_id) { return chunk_id * chunk_size; }
    bool load_chunk(uint32_t chunk_id);
};

extern ChunkedFontCache g_chunked_font_cache;
