# 配置管理器 (Config Manager)

## 概述

配置管理器是一个独立的模块，负责自动保存和恢复全局配置 `g_config` 到 SPIFFS 文件系统中的 `/spiffs/readpaper.cfg` 文件。

## 主要特性

- ✅ **自动初始化**：在系统启动时自动加载保存的配置
- ✅ **主动保存**：调用 config_save() 立即保存配置
- ✅ **版本控制**：支持配置文件版本管理和兼容性检查
- ✅ **统计信息**：跟踪保存/加载次数和时间戳
- ✅ **错误处理**：完善的错误处理和调试输出
- ✅ **可扩展性**：易于添加新的配置项

## 文件结构

```
src/config/
├── config_manager.h    # 配置管理器头文件
└── config_manager.cpp  # 配置管理器实现
```

## API 接口

### 核心函数

```cpp
// 初始化配置系统（在 setup() 中调用）
bool config_init();

// 保存配置到文件（配置变化时主动调用）
bool config_save();

// 手动加载配置
bool config_load();
```

### 辅助函数

```cpp
// 文件操作
bool config_file_exists();
bool config_delete();
bool config_get_file_info(size_t* file_size, unsigned long* last_modified);

// 配置管理
void config_reset_to_defaults();
ConfigStats config_get_stats();
```

## 使用方法

### 1. 系统集成（已完成）

在 `src/init/setup.cpp` 中：
```cpp
#include "config/config_manager.h"

void setup() {
    // ... 其他初始化代码 ...
    
    // 初始化配置管理器
    if (config_init()) {
        printBootTime("配置管理器初始化完成");
        // 应用加载的配置
        M5.Display.setRotation(g_config.rotation);
    }
}
```

在 `src/main.cpp` 主循环中：
```cpp
#include "config/config_manager.h"

void MainTask(void *pvParameters) {
    setup();
    
    for (;;) {
        // 主任务处理其他逻辑
        // 不再需要定期检查配置
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 2. 配置变化时的处理

当修改 `g_config` 的任何字段时，直接调用 `config_save()` 来保存配置：

```cpp
#include "config/config_manager.h"

// 修改屏幕旋转
g_config.rotation = new_rotation;
M5.Display.setRotation(g_config.rotation);

// 立即保存配置
config_save();
```

### 3. 添加新的配置项

要添加新的配置项，需要修改以下文件：

1. **在 `include/readpaper.h` 中扩展 GlobalConfig 结构体**：
```cpp
struct GlobalConfig {
    uint_fast8_t rotation = 2;        // 现有配置
    bool auto_brightness = true;      // 新配置项
    uint8_t font_scale = 100;         // 新配置项
    uint32_t sleep_timeout = 300000;  // 新配置项
};
```

2. **在 `config_manager.cpp` 中添加保存/加载逻辑**：

在 `config_save()` 函数中：
```cpp
config_file.printf("auto_brightness=%s\n", g_config.auto_brightness ? "true" : "false");
config_file.printf("font_scale=%d\n", g_config.font_scale);
config_file.printf("sleep_timeout=%lu\n", g_config.sleep_timeout);
```

在 `config_load()` 函数中：
```cpp
else if (key == "auto_brightness") {
    temp_config.auto_brightness = (value == "true");
}
else if (key == "font_scale") {
    int scale = value.toInt();
    if (scale >= 50 && scale <= 200) {
        temp_config.font_scale = scale;
    }
}
else if (key == "sleep_timeout") {
    temp_config.sleep_timeout = value.toInt();
}
```

在 `config_reset_to_defaults()` 函数中：
```cpp
g_config.auto_brightness = true;
g_config.font_scale = 100;
g_config.sleep_timeout = 300000;
```

## 配置文件格式

生成的配置文件示例：
```
# ReaderPaper 配置文件
# 版本: 1
# 生成时间: 12345678

version=1
rotation=2

# 文件结束
```

## 调试和测试

### 调试输出

设置 `DBG_CONFIG_MANAGER` 为 1 可以启用详细的调试输出：
```cpp
#define DBG_CONFIG_MANAGER 1
```

### 测试函数

已提供测试函数 `test::test_config_manager()`，可以在开发时调用来验证功能：
```cpp
#include "test/test_functions.h"

// 在适当位置调用
test::test_config_manager();
```

## 配置统计信息

可以通过 `config_get_stats()` 获取统计信息：
```cpp
ConfigStats stats = config_get_stats();
Serial.printf("保存次数: %lu, 加载次数: %lu\n", 
             stats.total_saves, stats.total_loads);
```

## 注意事项

1. **SPIFFS 依赖**：确保 SPIFFS 在配置管理器初始化前已正确挂载
2. **线程安全**：当前实现不是线程安全的，建议在主线程中使用
3. **存储限制**：SPIFFS 空间有限，避免存储大量数据
4. **主动保存**：配置变化后需要主动调用 `config_save()` 才会保存
5. **错误恢复**：如果配置文件损坏，系统会自动重置为默认配置

## 已集成的位置

- ✅ `src/init/setup.cpp` - 初始化调用
- ✅ `src/tasks/state_main_menu.cpp` - 屏幕旋转时主动保存
- ✅ `src/test/test_functions.cpp` - 测试函数