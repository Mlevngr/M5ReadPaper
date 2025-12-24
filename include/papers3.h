#define SD_SPI_CS_PIN 47
#define SD_SPI_SCK_PIN 39
#define SD_SPI_MOSI_PIN 38
#define SD_SPI_MISO_PIN 40

#define TOUCH_INT_PIN 48

#ifndef SOC_SDMMC_USE_GPIO_MATRIX
#define SOC_SDMMC_USE_GPIO_MATRIX
#endif

// === PaperS3 渲染可调参数（默认值） ===
// 这些宏暴露了 `bin_font_print` 内部用于缩放/下采样字形位图的调优项。
// 在这里修改可以快速在渲染质量与 CPU/PSRAM 开销之间做实验。
//
// 使用注意：
// - 默认值为在 PaperS3 硬件上取得质量与性能平衡而选定。
// - 降低阈值或增大采样范围可以在缩小时保留更细的笔画，但会增加计算量和渲染延迟。
// - 提高阈值或减少采样可以加速渲染，但可能导致细笔画丢失或小字号看起来更稀薄。
// - 逐步微调并对代表性字体（中英混合）和字号（例如 36、32、24、16）进行测试，找到适合你的配置。

// 请求时允许的最小/最大 scale_factor。bin_font_print 会将请求的 scale 限幅到此区间。
// 只有在确实需要允许更小或更大的缩放时才修改这些值。取值为浮点数。
// 建议安全范围：0.2 .. 3.0（过小的值可能导致字形不可读）。
#ifndef PAPERS3_SCALE_MIN
#define PAPERS3_SCALE_MIN 0.30f    // minimum scale_factor allowed
#endif
#ifndef PAPERS3_SCALE_MAX
#define PAPERS3_SCALE_MAX 2.00f    // maximum scale_factor allowed
#endif

// 细粒度下采样时使用的最大邻域半径。当渲染器将多个源像素映射到一个目标像素时
//（强缩小），会在源字形上采样一个小邻域来决定是否绘制该目标像素。更大的半径能
// 捕捉更多上下文，有助于保留短笔画或斜线，但会增加 CPU 成本。该值以源像素为单位，
// 为整数（典型取值：1-4）。
#ifndef PAPERS3_SAMPLE_RANGE_MAX
#define PAPERS3_SAMPLE_RANGE_MAX 8 // max neighborhood radius for downscale sampling
#endif

// 覆盖率（coverage）相关决策使用的基础阈值上下界。渲染器会为每个目标像素计算
// 覆盖率（源区域中黑色像素占比），并与动态阈值比较，宏定义用于限制该阈值范围。
// - 降低 PAPERS3_BASE_THRESHOLD_MIN：更容易绘制细笔画（绘制像素增多），但噪点风险上升。
// - 提高 PAPERS3_BASE_THRESHOLD_MAX：更保守的绘制（噪点少），但缩小时小笔画可能消失。
// 默认范围：0.05 .. 0.45。建议按小步（例如 0.05）调整并观察效果。
#ifndef PAPERS3_BASE_THRESHOLD_MIN
#define PAPERS3_BASE_THRESHOLD_MIN 0.01f // lowest allowed base threshold for coverage
#endif
#ifndef PAPERS3_BASE_THRESHOLD_MAX
#define PAPERS3_BASE_THRESHOLD_MAX 0.10f // highest allowed base threshold for coverage
#endif

// 快速调优建议：
// 1) 若小字号（<=16）在缩小时出现噪点：稍微增大 PAPERS3_BASE_THRESHOLD_MIN（例如 0.08），
//    或把 PAPERS3_SAMPLE_RANGE_MAX 降到 2。
// 2) 若中等字号（24..36）在缩小时丢失细笔画：降低 PAPERS3_BASE_THRESHOLD_MIN（例如 0.03->0.05），
//    或增大 PAPERS3_SAMPLE_RANGE_MAX。
// 3) 若渲染耗时过高：提高 PAPERS3_SCALE_MIN（限制极小缩放），减少 PAPERS3_SAMPLE_RANGE_MAX，
//    或考虑通过修改 `bin_font_print.cpp` 中的 SCALING_ALGORITHM 切换到更快的算法。
// 4) 发布固件时建议选择保守设置以避免闪烁，并在目标设备上保证单页渲染延迟 < 100ms。