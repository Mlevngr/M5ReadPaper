#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdint.h>
#include <cstddef>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 每任务内存池：避免并发渲染和索引任务共享缓冲区导致数据竞态
// 每个任务通过 FreeRTOS TaskHandle 自动获取独立的内存池实例
class MemoryPool
{
private:
    uint8_t *raw_buffer;
    uint16_t *bitmap_buffer;
    size_t raw_size;
    size_t bitmap_size;
    bool raw_in_use;
    bool bitmap_in_use;
    TaskHandle_t owner_task;  // 该池所属的任务句柄

public:
    MemoryPool();
    ~MemoryPool();

    uint8_t* get_raw_buffer(size_t size);
    void release_raw_buffer();
    
    uint16_t* get_bitmap_buffer(size_t pixel_count);
    void release_bitmap_buffer();
    
    void cleanup();
    
    // 设置拥有者任务（用于调试和验证）
    void set_owner_task(TaskHandle_t task) { owner_task = task; }
    TaskHandle_t get_owner_task() const { return owner_task; }
    
    // 获取当前任务的内存池实例（线程安全）
    static MemoryPool* get_task_pool();
    
    // 清理所有任务池（系统关闭时调用）
    static void cleanup_all_pools();
};

// 向后兼容：保留全局池引用，但内部会重定向到任务池
extern MemoryPool& g_memory_pool;

#endif // MEMORY_POOL_H
