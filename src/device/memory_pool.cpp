#include "memory_pool.h"
#include <M5Unified.h>
#include "test/per_file_debug.h"
#include <freertos/semphr.h>
#include <vector>

// 任务池注册表：使用vector+线性查找替代map（避免STL兼容性问题）
struct TaskPoolEntry {
    TaskHandle_t task;
    MemoryPool* pool;
};
static std::vector<TaskPoolEntry> g_task_pools;
static SemaphoreHandle_t g_pools_mutex = nullptr;
static MemoryPool g_fallback_pool;  // 备用池（用于未知任务或系统初始化阶段）

// 初始化互斥锁（首次调用时）
static void ensure_mutex_init() {
    if (!g_pools_mutex) {
        g_pools_mutex = xSemaphoreCreateMutex();
    }
}

MemoryPool::MemoryPool() : raw_buffer(nullptr), bitmap_buffer(nullptr), 
                           raw_size(0), bitmap_size(0), raw_in_use(false), bitmap_in_use(false),
                           owner_task(nullptr)
{
}

MemoryPool::~MemoryPool()
{
    cleanup();
}

uint8_t* MemoryPool::get_raw_buffer(size_t size)
{
    if (!raw_in_use && raw_buffer && raw_size >= size)
    {
        raw_in_use = true;
        return raw_buffer;
    }

    // 需要重新分配或首次分配
    if (raw_buffer && raw_size < size)
    {
        free(raw_buffer);
        raw_buffer = nullptr;
    }

    if (!raw_buffer)
    {
        // 使用PSRAM以节省内部DRAM（通常用于较大的缓冲区）
        raw_buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!raw_buffer) {
            // PSRAM分配失败，尝试内部RAM
            raw_buffer = (uint8_t *)malloc(size);
        }
        if (raw_buffer) {
            raw_size = size;
#if DBG_MEMORY_POOL
            Serial.printf("[MEMORY_POOL] raw_buffer 分配成功: addr=0x%08X, size=%u\n", 
                         (uint32_t)raw_buffer, size);
#endif
        } else {
#if DBG_MEMORY_POOL
            Serial.printf("[MEMORY_POOL] ⚠️ raw_buffer 分配失败! size=%u\n", size);
#endif
            return nullptr;
        }
    }

    raw_in_use = true;
    return raw_buffer;
}

void MemoryPool::release_raw_buffer()
{
    raw_in_use = false;
}

uint16_t* MemoryPool::get_bitmap_buffer(size_t pixel_count)
{
    size_t size = pixel_count * sizeof(uint16_t);
    if (!bitmap_in_use && bitmap_buffer && bitmap_size >= size)
    {
        bitmap_in_use = true;
        return bitmap_buffer;
    }

    // 需要重新分配或首次分配
    if (bitmap_buffer && bitmap_size < size)
    {
        free(bitmap_buffer);
        bitmap_buffer = nullptr;
    }

    if (!bitmap_buffer)
    {
        // 使用PSRAM以节省内部DRAM（图像缓冲区通常较大）
        bitmap_buffer = (uint16_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!bitmap_buffer) {
            // PSRAM分配失败，尝试内部RAM
            bitmap_buffer = (uint16_t *)malloc(size);
        }
        if (bitmap_buffer) {
            bitmap_size = size;
#if DBG_MEMORY_POOL
            Serial.printf("[MEMORY_POOL] bitmap_buffer 分配成功: addr=0x%08X, size=%u (pixels=%u)\n", 
                         (uint32_t)bitmap_buffer, size, size/2);
#endif
        } else {
#if DBG_MEMORY_POOL
            Serial.printf("[MEMORY_POOL] ⚠️ bitmap_buffer 分配失败! size=%u\n", size);
#endif
            return nullptr;
        }
    }

    bitmap_in_use = true;
    return bitmap_buffer;
}

void MemoryPool::release_bitmap_buffer()
{
    bitmap_in_use = false;
}

void MemoryPool::cleanup()
{
    if (raw_buffer)
    {
        free(raw_buffer);
        raw_buffer = nullptr;
    }
    if (bitmap_buffer)
    {
        free(bitmap_buffer);
        bitmap_buffer = nullptr;
    }
    raw_size = 0;
    bitmap_size = 0;
    raw_in_use = false;
    bitmap_in_use = false;
#if DBG_MEMORY_POOL
    Serial.printf("[MEMORY_POOL] 池已清理 (task=%p)\n", (void*)owner_task);
#endif
}

// 获取当前任务的内存池（自动创建）
MemoryPool* MemoryPool::get_task_pool()
{
    ensure_mutex_init();
    
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (!current_task) {
#if DBG_MEMORY_POOL
        Serial.println("[MEMORY_POOL] ⚠️ 无法获取当前任务句柄，使用备用池");
#endif
        return &g_fallback_pool;
    }
    
    // 尝试从注册表查找现有池
    if (xSemaphoreTake(g_pools_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 线性查找当前任务的池
        for (auto& entry : g_task_pools) {
            if (entry.task == current_task) {
                xSemaphoreGive(g_pools_mutex);
                return entry.pool;
            }
        }
        
        // 为新任务创建独立池
        MemoryPool* new_pool = new MemoryPool();
        if (new_pool) {
            new_pool->set_owner_task(current_task);
            TaskPoolEntry new_entry;
            new_entry.task = current_task;
            new_entry.pool = new_pool;
            g_task_pools.push_back(new_entry);
#if DBG_MEMORY_POOL
            Serial.printf("[MEMORY_POOL] ✅ 为任务 %p 创建新池 (%zu 个活跃池)\n", 
                         (void*)current_task, g_task_pools.size());
#endif
        }
        xSemaphoreGive(g_pools_mutex);
        return new_pool ? new_pool : &g_fallback_pool;
    }
    
#if DBG_MEMORY_POOL
    Serial.println("[MEMORY_POOL] ⚠️ 获取互斥锁超时，使用备用池");
#endif
    return &g_fallback_pool;
}

// 清理所有任务池（系统关闭时调用）
void MemoryPool::cleanup_all_pools()
{
    ensure_mutex_init();
    
    if (xSemaphoreTake(g_pools_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
#if DBG_MEMORY_POOL
        Serial.printf("[MEMORY_POOL] 清理 %zu 个任务池...\n", g_task_pools.size());
#endif
        for (auto& entry : g_task_pools) {
            if (entry.pool) {
                entry.pool->cleanup();
                delete entry.pool;
            }
        }
        g_task_pools.clear();
        g_fallback_pool.cleanup();
        xSemaphoreGive(g_pools_mutex);
#if DBG_MEMORY_POOL
        Serial.println("[MEMORY_POOL] ✅ 所有任务池已清理");
#endif
    }
}

// 向后兼容的全局引用（重定向到当前任务池）
static MemoryPool& get_legacy_pool() {
    return *MemoryPool::get_task_pool();
}

MemoryPool& g_memory_pool = get_legacy_pool();
