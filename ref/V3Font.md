## 设备侧 SD 卡字体处理逻辑分析与 V3 字体兼容性指南

### 一、架构概览

设备侧字体处理分为以下几个核心层次：

```
┌──────────────────────────────────────────────────────────────┐
│                      应用层 (UI/Text Render)                  │
│  bin_font_print.cpp / text_handle.cpp / book_handle.cpp      │
├──────────────────────────────────────────────────────────────┤
│                      字体解码层                               │
│  font_decoder.cpp  ←─ 负责 V2(1bit) / V3(2bit Huffman) 解码  │
│  font_color_mapper.cpp ←─ 像素值到 RGB565 颜色映射            │
├──────────────────────────────────────────────────────────────┤
│                      缓存管理层                               │
│  chunked_font_cache.cpp (分块缓存)                           │
│  memory_pool.cpp (内存池)                                    │
│  font_header_cache (头部缓存)                                │
├──────────────────────────────────────────────────────────────┤
│                      存储层                                   │
│  SD / SPIFFS / PROGMEM                                        │
└──────────────────────────────────────────────────────────────┘
```

---

### 二、各层详细分析

#### 2.1 字体文件格式检测 (bin_font_print.cpp)

```cpp
FontFormat detect_font_format(File &f) {
    // 头部结构: char_count(4) + font_height(1) + version(1) + family_name(64) + style_name(64)
    uint8_t version = header[5];
    
    // 版本2: V2 字体 (1bit 格式)
    if (version == 2) return FONT_FORMAT_1BIT;
    
    // 版本3: V3 字体 (2bit Huffman 编码)
    if (version == 3) return FONT_FORMAT_HUFFMAN;
}
```

**✅ V3 支持状态**：已支持检测 V3 字体格式。

#### 2.2 缓存机制

| 缓存层 | 文件 | 作用 | V3 兼容 |
|--------|------|------|---------|
| 头部缓存 | bin_font_print.cpp | 缓存 134 字节头部到 PSRAM | ✅ 兼容 |
| 分块缓存 | `chunked_font_cache.cpp` | 将整个字体文件分块存储到内存 | ✅ 兼容 |
| 内存池 | `memory_pool.cpp` | 管理位图解码临时缓冲区 | ✅ 兼容 |

缓存层对 V3 字体透明，无需修改。

#### 2.3 字体解码器 (font_decoder.cpp)

支持三种解码模式：

| 解码函数 | 字体版本 | 编码规则 |
|----------|----------|----------|
| `decode_bitmap_1bit()` | V2 | 1bit 黑白: 0=黑, 1=白 |
| `decode_bitmap()` | V2 (旧) | Huffman 黑白 |
| **`decode_bitmap_v3()`** | V3 | 2bit Huffman: 0=白, 11=黑, 10=灰 |

**V3 解码核心逻辑**（font_decoder.cpp）：

```cpp
void FontDecoder::decode_bitmap_v3(...) {
    while (pixel_idx < w * h && byte_pos < bitmap_size) {
        uint8_t first_bit = (current_byte >> (7 - bit_pos)) & 0x01;
        
        if (first_bit == 0) {
            // 编码 '0' → 白色背景
            pixel_value = PIXEL_WHITE;
        } else {
            // 读取第二位
            uint8_t second_bit = ...;
            if (second_bit == 0) {
                // 编码 '10' → 灰色像素
                pixel_value = PIXEL_GRAY;
            } else {
                // 编码 '11' → 黑色前景
                pixel_value = PIXEL_BLACK;
            }
        }
        bitmap[pixel_idx++] = FontColorMapper::map_v3_color(pixel_value, dark_mode, transparent);
    }
}
```

**✅ V3 支持状态**：解码器完整支持 V3 的 3 阶灰度。

#### 2.4 颜色映射器 (font_color_mapper.cpp)

| 像素值 | Normal Mode | Dark Mode |
|--------|-------------|-----------|
| `PIXEL_WHITE (0)` | 0xFFFF (白) 或透明 | 0x0000 (黑) 或透明 |
| `PIXEL_BLACK (1)` | 0x0000 (黑) | 0xFFFF (白) |
| `PIXEL_GRAY (2)` | `GREY_MAP_COLOR` (0x8430) | `TFT_LIGHTGREY` |

**✅ V3 支持状态**：颜色映射完整支持 3 阶灰度。

#### 2.5 渲染路径 (bin_font_print.cpp)

渲染代码中已区分 V2/V3 字体的处理：

```cpp
// 根据字体版本选择解码器
if (g_bin_font.version == 3) {
    FontDecoder::decode_bitmap_v3(raw_data, glyph->bitmap_size, char_bitmap, ...);
} else if (g_bin_font.format == FONT_FORMAT_1BIT) {
    FontDecoder::decode_bitmap_1bit(...);
} else {
    FontDecoder::decode_bitmap(...);  // 旧版 Huffman
}

// 渲染时区分处理
if (g_bin_font.version == 3) {
    // V3: 直接使用解码后的颜色（包括灰度）
    target_canvas->pushImage(canvas_x, canvas_y, width, height, char_bitmap);
} else {
    // V2: 需要转换颜色
    for (size_t i = 0; i < pixels; ++i) {
        rgb_buf[i] = (p != 0xFFFF) ? text_color : bg_color;
    }
}
```

**✅ V3 支持状态**：渲染路径已支持 V3 灰度直接输出。

---

### 三、⚠️ V3 字体缩放时的问题分析

**这是目前 V3 兼容的关键短板！**

查看缩放代码（bin_font_print.cpp）：

```cpp
// 缩放版本：需要处理颜色和缩放
for (int16_t sy = 0; sy < scaled_height; sy++) {
    for (int16_t sx = 0; sx < scaled_width; sx++) {
        int16_t orig_x = (int16_t)(sx / scale_factor);
        int16_t orig_y = (int16_t)(sy / scale_factor);
        
        uint16_t pixel = char_bitmap[orig_y * render_width + orig_x];
        
        if (g_bin_font.version == 3) {
            // V3: 直接使用解码后的颜色
            uint16_t bg_color = FontColorMapper::get_background_color(dark);
            if (pixel != bg_color) {
                target_canvas->drawPixel(canvas_x + sx, canvas_y + sy, pixel);
            }
        }
    }
}
```

**问题**：当前缩放算法使用**最近邻插值**，对于 V3 的灰度像素存在以下问题：

| 问题 | 描述 |
|------|------|
| 灰度丢失 | 最近邻算法直接取最近像素，可能跳过灰度抗锯齿像素 |
| 灰度混合缺失 | 放大时未考虑灰度像素的平滑过渡 |
| 缩小时灰度采样不当 | 多个灰度像素合并时应计算加权平均而非简单二值化 |

---

### 四、webapp 生成的 V3 字体支持总结

| 功能模块 | 支持状态 | 说明 |
|----------|----------|------|
| 字体格式检测 | ✅ 完整支持 | 正确识别 version=3 |
| 头部解析 | ✅ 完整支持 | 134 字节头部兼容 |
| 缓存机制 | ✅ 完整支持 | 对字体版本透明 |
| V3 解码器 | ✅ 完整支持 | `decode_bitmap_v3()` 实现完整 |
| 颜色映射 | ✅ 完整支持 | 3 阶灰度正确映射 |
| **无缩放渲染** | ✅ 完整支持 | `pushImage` 直接输出灰度 |
| **缩放渲染** | ⚠️ 部分支持 | 灰度插值算法需改进 |
| 竖排模式 | ✅ 完整支持 | 与横排同等支持 |
| 暗黑模式 | ✅ 完整支持 | 灰度反色正确 |

---

### 五、完美兼容 V3 字体的改进步骤

#### 步骤 1：改进缩放算法以支持灰度插值

**修改文件**：bin_font_print.cpp

**核心思路**：在缩放时，对灰度像素进行加权平均而非简单的最近邻采样。

**建议实现**：

```cpp
// 新增：V3 灰度感知的缩放渲染函数
void render_v3_scaled(M5Canvas* canvas, uint16_t* bitmap, 
                      int16_t orig_w, int16_t orig_h,
                      int16_t scaled_w, int16_t scaled_h,
                      int16_t canvas_x, int16_t canvas_y,
                      float scale_factor, bool dark_mode) {
    
    uint16_t bg_color = FontColorMapper::get_background_color(dark_mode);
    
    for (int16_t sy = 0; sy < scaled_h; sy++) {
        for (int16_t sx = 0; sx < scaled_w; sx++) {
            // 计算原图对应区域
            float orig_x_start = sx / scale_factor;
            float orig_y_start = sy / scale_factor;
            float orig_x_end = (sx + 1) / scale_factor;
            float orig_y_end = (sy + 1) / scale_factor;
            
            // 采样区域内的所有像素
            float total_weight = 0.0f;
            float gray_sum = 0.0f;
            bool has_content = false;
            
            for (int16_t oy = (int16_t)orig_y_start; oy <= (int16_t)orig_y_end && oy < orig_h; oy++) {
                for (int16_t ox = (int16_t)orig_x_start; ox <= (int16_t)orig_x_end && ox < orig_w; ox++) {
                    // 计算重叠面积作为权重
                    float overlap_x = fminf(orig_x_end, ox + 1) - fmaxf(orig_x_start, ox);
                    float overlap_y = fminf(orig_y_end, oy + 1) - fmaxf(orig_y_start, oy);
                    float weight = overlap_x * overlap_y;
                    
                    uint16_t pixel = bitmap[oy * orig_w + ox];
                    if (pixel == bg_color) continue;
                    
                    has_content = true;
                    
                    // 将像素转换为灰度值 (0=黑, 0.5=灰, 1=白)
                    float gray_val;
                    if (pixel == 0x0000 || pixel == 0xFFFF) {  // 黑或白
                        gray_val = (pixel == 0x0000) ? 0.0f : 1.0f;
                    } else {
                        // 灰度像素 - 估算中间值
                        gray_val = 0.5f;
                    }
                    
                    gray_sum += gray_val * weight;
                    total_weight += weight;
                }
            }
            
            if (has_content && total_weight > 0.0f) {
                float avg_gray = gray_sum / total_weight;
                
                // 根据平均灰度决定输出颜色
                uint16_t output_color;
                if (avg_gray < 0.25f) {
                    // 偏黑 → 黑色
                    output_color = dark_mode ? 0xFFFF : 0x0000;
                } else if (avg_gray < 0.75f) {
                    // 中间 → 灰色
                    output_color = dark_mode ? TFT_LIGHTGREY : GREY_MAP_COLOR;
                } else {
                    // 偏白 → 跳过（透明/背景）
                    continue;
                }
                
                canvas->drawPixel(canvas_x + sx, canvas_y + sy, output_color);
            }
        }
    }
}
```

#### 步骤 2：修改渲染调用点

**位置**：bin_font_print.cpp 和 bin_font_print.cpp

将：
```cpp
if (g_bin_font.version == 3) {
    // V3: 直接使用解码后的颜色
    // ... 最近邻缩放代码
}
```

改为：
```cpp
if (g_bin_font.version == 3) {
    render_v3_scaled(target_canvas, char_bitmap,
                     render_width, render_height,
                     scaled_width, scaled_height,
                     canvas_x, canvas_y, scale_factor, dark);
}
```

#### 步骤 3：优化灰度颜色常量

**文件**：readpaper.h

当前只有一个灰度值 `GREY_MAP_COLOR`。可考虑增加更多灰度级别以支持更精细的抗锯齿：

```cpp
// V3 字体灰度映射 (可选扩展)
#define GREY_LEVEL_LIGHT  0xC618  // 浅灰 ~75%
#define GREY_LEVEL_MID    0x8430  // 中灰 ~50%
#define GREY_LEVEL_DARK   0x4208  // 深灰 ~25%
```

#### 步骤 4：测试用例

创建测试脚本验证以下场景：

1. **无缩放渲染**：确认灰度像素正确显示
2. **放大渲染** (scale > 1.0)：灰度边缘应平滑
3. **缩小渲染** (scale < 1.0)：灰度像素应正确合并
4. **暗黑模式**：灰度反色正确
5. **竖排模式**：旋转后灰度保持

---

### 六、改进优先级建议

| 优先级 | 任务 | 工作量 | 影响 |
|--------|------|--------|------|
| P0 | 修复缩放时的灰度插值 | 中 | 解决 V3 字体在非 1:1 显示时的核心问题 |
| P1 | 增加灰度级别常量 | 低 | 提升视觉质量 |
| P2 | 性能优化（SIMD/批量绘制） | 高 | 改善缩放渲染性能 |
| P3 | 添加 V3 字体专用测试页面 | 低 | 便于验证和调试 |

---

### 七、总结

**当前状态**：设备侧已具备 V3 字体的**基础支持**，包括格式检测、解码、颜色映射和无缩放渲染。

**主要短板**：**缩放渲染时灰度像素处理不当**，导致抗锯齿效果在字体大小调整时丢失或失真。

**下一步**：实现灰度感知的缩放算法，确保 V3 字体的 3 阶灰度在任何字体大小下都能正确呈现。