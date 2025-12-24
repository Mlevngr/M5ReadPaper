#include "chunked_font_cache.h"
#include <algorithm>
#include "test/per_file_debug.h"

ChunkedFontCache g_chunked_font_cache;

ChunkedFontCache::ChunkedFontCache()
    : font_file(nullptr), total_font_size(0), chunk_size(0), total_chunks(0), 
      fully_loaded(false), successful_chunks(0), failed_chunks(0), total_allocated(0)
{
}

ChunkedFontCache::~ChunkedFontCache()
{
    cleanup();
}

bool ChunkedFontCache::load_entire_font_chunked(File& fontFile, size_t chunk_kb)
{
    cleanup();  // 清理之前的数据
    
    font_file = &fontFile;
    
    // 获取字体文件大小
    fontFile.seek(0, SeekEnd);
    total_font_size = fontFile.position();
    fontFile.seek(0, SeekSet);
    
    chunk_size = chunk_kb * 1024;
    const size_t kMinChunk = 32 * 1024;
    const size_t kAlign = 4 * 1024;
    if (chunk_size < kMinChunk)
    {
        chunk_size = kMinChunk;
    }
    if (chunk_size % kAlign != 0)
    {
        chunk_size = ((chunk_size + kAlign - 1) / kAlign) * kAlign;
    }
    total_chunks = (total_font_size + chunk_size - 1) / chunk_size;  // 向上取整
    
#if DBG_CHUNKED_FONT_CACHE
    Serial.printf("[CHUNKED_CACHE] === 分块缓存加载 ===\n");
    Serial.printf("[CHUNKED_CACHE] 字体文件: %.2fMB, 块大小: %uKB\n", 
                 total_font_size / (1024.0 * 1024.0), chunk_kb);
    Serial.printf("[CHUNKED_CACHE] 总块数: %u, PSRAM可用: %.2fMB\n", 
                 total_chunks, heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / (1024.0 * 1024.0));
    Serial.printf("[CHUNKED_CACHE] 使用原始数据存储（无压缩）\n");
#endif
    
    // 预分配chunks数组
    chunks.resize(total_chunks);
    for (uint32_t i = 0; i < total_chunks; i++) {
        chunks[i].data = nullptr;
        chunks[i].size = 0;
        chunks[i].original_size = 0;
        chunks[i].loaded = false;
        chunks[i].compressed_1bit = false;
    }
    
    // 逐块加载整个字体文件
    successful_chunks = 0;
    failed_chunks = 0;
    total_allocated = 0;
    compression_saved = 0;
    enable_1bit_compression = false;  // 禁用压缩
    
#if DBG_CHUNKED_FONT_CACHE
    Serial.printf("[CHUNKED_CACHE] 开始逐块加载...\n");
#endif
    unsigned long start_time = millis();
    (void)start_time;  // 抑制未使用变量警告
    
    for (uint32_t chunk_id = 0; chunk_id < total_chunks; chunk_id++) {
        if (load_chunk(chunk_id)) {  // 使用标准加载，不压缩
            successful_chunks++;
        } else {
            failed_chunks++;
#if DBG_CHUNKED_FONT_CACHE
            Serial.printf("[CHUNKED_CACHE] 块 %u/%u 加载失败\n", chunk_id + 1, total_chunks);
#endif
        }
        
        // 每10块报告一次进度
        if ((chunk_id + 1) % 10 == 0 || chunk_id == total_chunks - 1) {
#if DBG_CHUNKED_FONT_CACHE
            Serial.printf("[CHUNKED_CACHE] 进度: %u/%u (%.1f%%), 成功: %u, 失败: %u\n",
                         chunk_id + 1, total_chunks, 
                         (float)(chunk_id + 1) * 100.0f / total_chunks,
                         successful_chunks, failed_chunks);
            Serial.printf("[CHUNKED_CACHE] 内存使用: %.2fMB\n",
                         total_allocated / (1024.0 * 1024.0));
#endif
        }
    }
    
#if DBG_CHUNKED_FONT_CACHE
    unsigned long load_time = millis() - start_time;
#endif
    fully_loaded = (failed_chunks == 0);
    
#if DBG_CHUNKED_FONT_CACHE
    Serial.printf("[CHUNKED_CACHE] === 分块加载完成 ===\n");
    Serial.printf("[CHUNKED_CACHE] 加载时间: %lu ms\n", load_time);
    Serial.printf("[CHUNKED_CACHE] 成功块: %u/%u (%.1f%%)\n", 
                 successful_chunks, total_chunks,
                 (float)successful_chunks * 100.0f / total_chunks);

    if (fully_loaded) {
        Serial.printf("[CHUNKED_CACHE] ✅ 完整字体已分块缓存\n");
    } else {
        Serial.printf("[CHUNKED_CACHE] ⚠️  部分块加载失败，将混合使用缓存和文件访问\n");
    }
#endif
    
    return successful_chunks > 0;  // 只要有部分成功就返回true
}

bool ChunkedFontCache::read_data(uint32_t offset, uint8_t* dest, uint32_t size)
{
    if (!font_file || !dest || size == 0) {
        return false;
    }
    
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        uint32_t current_offset = offset + bytes_read;
        uint32_t chunk_id = get_chunk_id(current_offset);
        
        // 检查chunk_id是否有效
        if (chunk_id >= total_chunks) {
            return false;
        }
        
        uint32_t chunk_start = get_chunk_offset(chunk_id);
        uint32_t offset_in_chunk = current_offset - chunk_start;
        uint32_t bytes_in_this_chunk = std::min(size - bytes_read, 
                                                chunk_size - offset_in_chunk);
        
        if (chunks[chunk_id].loaded && chunks[chunk_id].data) {
            // 从缓存读取 - 直接拷贝原始数据
            memcpy(dest + bytes_read, chunks[chunk_id].data + offset_in_chunk, bytes_in_this_chunk);
        } else {
            // 直接文件读取（块加载失败的情况）
            font_file->seek(current_offset);
            if (font_file->read(dest + bytes_read, bytes_in_this_chunk) != bytes_in_this_chunk) {
                return false;
            }
        }
        
        bytes_read += bytes_in_this_chunk;
    }
    
    return true;
}

bool ChunkedFontCache::load_chunk(uint32_t chunk_id)
{
    if (chunk_id >= total_chunks) {
        return false;
    }
    
    uint32_t chunk_offset = get_chunk_offset(chunk_id);
    size_t this_chunk_size = chunk_size;
    
    // 最后一块可能比标准块小
    if (chunk_id == total_chunks - 1) {
        this_chunk_size = total_font_size - chunk_offset;
    }
    
    // 尝试分配内存 - 优先使用PSRAM，回退到普通内存
    uint8_t* chunk_data = (uint8_t*)heap_caps_malloc(this_chunk_size, MALLOC_CAP_SPIRAM);
    if (!chunk_data) {
    chunk_data = (uint8_t*)malloc(this_chunk_size);
        if (!chunk_data) {
            return false;  // 分配失败
        }
    }
    
    // 从文件读取数据
    if (font_file->position() != chunk_offset)
    {
        font_file->seek(chunk_offset);
    }
    size_t bytes_read = font_file->read(chunk_data, this_chunk_size);
    
    if (bytes_read != this_chunk_size) {
#if DBG_CHUNKED_FONT_CACHE
        Serial.printf("[MEM] free chunk_data due to read failure (size=%u): heap_free=%u, psram_free=%u\n", this_chunk_size, esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
        free(chunk_data);
        return false;  // 读取失败
    }
    
    // 保存块信息
    chunks[chunk_id].data = chunk_data;
    chunks[chunk_id].size = this_chunk_size;
    chunks[chunk_id].loaded = true;
    total_allocated += this_chunk_size;
    
    return true;
}

void ChunkedFontCache::cleanup()
{
    for (auto& chunk : chunks) {
        if (chunk.data) {
#if DBG_CHUNKED_FONT_CACHE
            Serial.printf("[MEM] free chunk.data (size=%u) during cleanup: heap_free=%u, psram_free=%u\n", chunk.size, esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
            free(chunk.data);
            chunk.data = nullptr;
        }
        chunk.loaded = false;
        chunk.size = 0;
    }
    chunks.clear();
    
    total_allocated = 0;
    successful_chunks = 0;
    failed_chunks = 0;
    compression_saved = 0;
    enable_1bit_compression = false;
    fully_loaded = false;
    
#if DBG_CHUNKED_FONT_CACHE
    Serial.printf("[CHUNKED_CACHE] 分块缓存已清理\n");
#endif
}

void ChunkedFontCache::print_stats()
{
#if DBG_CHUNKED_FONT_CACHE
    Serial.printf("[CHUNKED_CACHE] === 分块缓存统计 ===\n");
    Serial.printf("[CHUNKED_CACHE] 总块数: %u, 成功: %u, 失败: %u\n", 
                 total_chunks, successful_chunks, failed_chunks);
    Serial.printf("[CHUNKED_CACHE] 完整率: %.1f%% (%s)\n", 
                 (float)successful_chunks * 100.0f / total_chunks,
                 fully_loaded ? "完整" : "部分");
    
    Serial.printf("[CHUNKED_CACHE] 内存使用: %.2fMB (块大小: %uKB)\n", 
                 total_allocated / (1024.0 * 1024.0), chunk_size / 1024);
    
    Serial.printf("[CHUNKED_CACHE] 预期性能: %s\n",
                 fully_loaded ? "接近一体化缓存" : "混合模式");
                Serial.printf("[CHUNKED_CACHE] 当前堆剩余: %.2fMB, PSRAM剩余: %.2fMB\n",
                              esp_get_free_heap_size() / (1024.0 * 1024.0),
                              heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / (1024.0 * 1024.0));
                Serial.printf("[CHUNKED_CACHE] 已分配给缓存的内存: %.2fMB\n",
                              total_allocated / (1024.0 * 1024.0));
#endif
}
