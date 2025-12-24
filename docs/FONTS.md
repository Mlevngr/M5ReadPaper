**字体处理流程文档（摘要）**

本文档概述本项目中字体相关的端到端流程：字体如何由 webapp/工具生成、在固件中以何种二进制结构存储、固件端如何读取并解码渲染、以及当前的字体缓存策略与优化点。

**一、字体生成（概要）**

  - 使用 `opentype.js`（见 `webapp/extension/vendors/opentype.min.js`）解析 TTF/OTF 字体并获取 glyph。
  - 在页面/Worker 中通过 Canvas 按需渲染字形灰度（见 `webapp/extension/pages/readpaper_renderer.js`）。
  - 渲染结果经过阈值二值化、可选平滑/去噪/膨胀，再打包为 1-bit 位图（函数 `ReadPaperRenderer._pack1Bit`），得到与固件兼容的位图字节流。
  - webapp 也可生成演示 PNG（unpack/preview 功能），方便人工校验字体效果。
  - 脚本可批量生成 `.bin` 字体文件；同时包含 `build_charset()` 用于生成字符集（GBK / Big5 / 日文 / ASCII 等）。
  - 项目约定：charset 的生成仍依赖该脚本（或类脚本），因此在说明中请勿只看 `tools/` JS 以外的实现。 (Till Ext V1.6.1, all later version relying on JS only)

  - 为了生成ReadPaper的PROGMEM字体，在扩展的生成页面，控制台输入 `showmethemoney(true)` 来打开隐藏的勾选项目

**二、字体文件（二进制）格式与数据结构**

  - char_count (uint32_t, 4 bytes)
  - font_height (uint8_t, 1 byte)
  - version (uint8_t, 1 byte)
  - family_name (64 bytes, utf-8 nul-padded)
  - style_name (64 bytes, utf-8 nul-padded)

  - `uint16_t unicode` (2)
  - `uint16_t width` (advance, 2)
  - `uint8_t bitmapW` (1)
  - `uint8_t bitmapH` (1)
  - `int8_t x_offset` (1)
  - `int8_t y_offset` (1)
  - `uint32_t bitmap_offset` (4)
  - `uint32_t bitmap_size` (4)
  - （C 结构中保留 `cached_bitmap` 字段，用于内存缓存或占位）

  - 有两类实现/版本：
    - 简单 1-bit 打包（Webapp/py 脚本生成）：每行按 MSB-first 打包；文件写入前 `pack1Bit` 会对最后一个字节做掩码并对行字节取反（JS 实现与 Python 实现一致）；C 端可以通过 `FontDecoder::decode_bitmap_1bit` 或 `unpack_1bit_bitmap` 解包得到 0/255 灰度像素。
    - 复杂编码（V3/Huffman）：支持更高位深或 2-bit/Huffman 编码以表达灰度，C 端解码器实现为 `FontDecoder::decode_bitmap`/`decode_bitmap_v3`（见 `src/text/font_decoder.cpp`），用于 V3 或未来复杂编码格式。

**三、固件端渲染流程（调用链与关键函数）**

**注意，下面提到的整体缓存方式事实上已经废弃不用，仅存在于v1.6之前的老版本之上**

  - 检测格式（`detect_font_format`），读取并缓存头部（PSRAM 头缓存）。
  - 两种运行模式：
    - 流式模式（`g_font_stream_mode = true`）：只加载轻量索引（`GlyphIndex`），并建立 `indexMap`（hash map）用于 O(1) 查找。位图按需从文件/PROGMEM 读取。
    - 缓存模式（非流式）：把整个字符表解析到 `g_bin_font.chars`（`BinFontChar` vector），并通过 `chunked_font_cache` 等组件缓存位图。 （Only in olderversions less than V1.6 ）


  - 若流式并使用外部存储（SPIFFS/SD），采用三段策略：
    1. 字形预读窗口（`GlyphReadWindow`，PSRAM 缓冲，256KB 可配置）命中则 memcpy 返回。
    2. 若窗口未命中，尝试 `reposition_window()` 将包含请求偏移的数据读入窗口，再 memcpy。
    3. 若上述皆失败，直接 `readAtOffset` 从 SD/文件读取。
  - 若使用 PROGMEM（内置字体），从 flash 读取；若使用缓存模式，走 `g_chunked_font_cache`。



**四、字体缓存机制（细节与路径）**

  1. 头部缓存（Header Cache）: 在 `load_bin_font` 里把 134 字节头部缓存到 PSRAM（`g_font_header_cache`），避免频繁的小量 I/O。
  2. 流式索引（轻量索引）: `g_bin_font.index` 与 `indexMap`（`GlyphIndex`）仅包含每字形的度量与位图偏移/长度（内存占用小），用于按需读取位图。适合内存受限或大字符集场景。
  3. 位图预读/分块缓存：
     - `GlyphReadWindow`：预读窗口（默认 256KB）把文件的一段数据缓存到 PSRAM，显著减少 SD 卡随机 seek。
     - `chunked_font_cache`（分块缓存）：在缓存模式下使用，能把位图拆分到较小块并放入 PSRAM，支持快速随机访问。
     - `PageFontCache` / `FontBufferManager`：为当前页面或常用字符构建单页/通用缓存，把需要的字形打包成连续内存块（减少绘制时 IO），适合界面渲染场景（例如目录/书签页）。

  - 若可用且为默认字体且 `fontLoadLoc==1`，优先使用 PROGMEM（内置）字体以避免 I/O 开销（`load_bin_font_from_progmem`）。
  - 对 SD 文件：优先使用 `GlyphReadWindow`（窗口命中）-> 重定位窗口 -> 直接读三段策略，结合互斥锁保护 `fontFile` 的并发访问。
  - 页面渲染前可通过 `PageFontCache::build()` 提前把该页用到的字形拉入 PSRAM 缓冲。

**五、注意事项与优化建议**

  + From Ext V1.6.2, all charset is handled by JS (and then python is not useful anymore but just for archieve)

**六、快速参考（关键文件）**



**附录：GBK → Unicode 映射表（generate_gbk_table.py）**

- **脚本位置与作用**：`tools/generate_gbk_table.py`。用于生成固件可直接包含的 GBK→Unicode 查找表（C++ 源文件），脚本支持两种模式：`--full` 生成完整映射，`--simple` 生成常用简化表。
  - 生成完整表并写入源码文件：
    - `python tools/generate_gbk_table.py --full > src/text/gbk_unicode_data.cpp`
    - `python tools/generate_gbk_table.py --simple > src/text/gbk_unicode_data.cpp`

  - 同时会定义 `const size_t GBK_TABLE_SIZE` 表示条目数。脚本会在 stderr 输出生成时的日志与估算的 Flash 占用（条目数 * 4 字节）。

- **在固件中的结构与访问**：
  - 头文件 [`src/text/gbk_unicode_table.h`](src/text/gbk_unicode_table.h) 中声明了：
    - `struct GBKToUnicodeEntry { uint16_t gbk_code; uint16_t unicode; };`
    - `extern const GBKToUnicodeEntry gbk_to_unicode_table[];`
    - `extern const size_t GBK_TABLE_SIZE;`
    - 查表/转换函数声明：`uint16_t gbk_to_unicode_lookup(uint16_t)`, `int utf8_encode(uint16_t, uint8_t*)`, `std::string convert_gbk_to_utf8_lookup(const std::string&)` 等。
  - 实现文件 [`src/text/gbk_unicode_table.cpp`](src/text/gbk_unicode_table.cpp) 使用 `pgm_read_word` 从 PROGMEM 读取表项并用二分查找（`gbk_to_unicode_lookup`）进行映射查找；字符串转换函数 `convert_gbk_to_utf8_lookup` 演示了如何把 GBK 字节流按双字节切分并查表再转成 UTF-8。

- **示例：在 C++ 中使用生成的表**：

  - 直接把脚本生成的 `gbk_unicode_data.cpp` 放到 `src/text/`（或其他源码目录）并加入到工程编译，随后在代码中：

    #include "gbk_unicode_table.h"

    // 把 GBK 原始字节串转换为 UTF-8
    std::string gbk_input = ...; // 包含 GBK 编码的字节
    std::string utf8 = convert_gbk_to_utf8_lookup(gbk_input);

    // 或者按单个 GBK 码查表并编码为 UTF-8
    uint16_t gbk_code = (high_byte << 8) | low_byte;
    uint16_t unicode = gbk_to_unicode_lookup(gbk_code);
    if (unicode) {
        uint8_t buf[4];
        int len = utf8_encode(unicode, buf);
        std::string s((char*)buf, len);
        // 将 s 追加到输出
    }

- **体积与性能注意**：
  - 完整表条目数较多（脚本会在 stderr 打印实际条目数），每项 4 字节，完整表通常会消耗若干十 KB 到上百 KB 的 Flash（脚本会给出估算）。
  - 表以只读 Flash（PROGMEM）方式存储，查表使用二分查找（O(log N)）读取 `pgm_read_word`，对实时性要求高的场景请注意查表开销；可考虑：
    - 仅生成并包含常用子集（`--simple` 或自定义裁剪）；
    - 在运行期缓存常用映射到 RAM（若 RAM 足够）。

- **集成建议**：
  - 将 `python tools/generate_gbk_table.py --full > src/text/gbk_unicode_data.cpp` 加入到构建前的生成步骤（例如 Makefile / pre-build 脚本），避免把生成后的大文件直接提交到版本库；或将其加入 `.gitignore` 并在 CI/构建环境中生成。 
  - 若需要减小 Flash 占用，可只生成常用字符子集或在固件中使用更轻量的替代（例如运行时使用 icon/替代符号）。

（结束）

**附录：繁简转换（gen_zh_table.py）**

- **脚本位置与作用**：`tools/gen_zh_table.py`。读取 `tools/zh_conv_table.csv`（4 列：Source,Target,SourceType,TargetType），提取繁体↔简体对并生成 C++ 源文件 `src/text/zh_conv_table_generated.cpp`，包含：
  - 一个字符串对数组 `zh_conv_pairs`（每项 `trad` / `simp`），
  - 两个按繁体与简体排序的索引数组 `zh_conv_indices_trad` / `zh_conv_indices_simp`，
  - 一个 BMP 范围内单字快速索引表 `zh_single_index`（O(1) 单字符查找），
  - 一个 C API `zh_conv_embedded_lookup(const char* key, uint8_t mode)`，mode=1 表示 繁->简，mode=2 表示 简->繁。

- **如何生成**：在仓库根目录下运行：

  ```bash
  # 生成并写入目标源码（覆盖/生成文件）
  python tools/gen_zh_table.py
  # 或显式指定输出文件（脚本默认输出到 src/text/zh_conv_table_generated.cpp）
  ```

  - 脚本会读取 `tools/zh_conv_table.csv` 并在 stdout/stderr 输出处理统计；最终在 `src/text/` 写入 `zh_conv_table_generated.cpp`。
  - 建议把该命令加入 pre-build 或 CI，以便在 CSV 更新后自动重建生成文件，避免把大文件直接手动维护。

- **输出文件要点**：
  - `zh_conv_pairs` 中每个字符串字面量为 UTF-8 编码的 C 字符串（`const char*`），通过索引数组按需二分查找以支持多字节串匹配。
  - `zh_single_index` 长度为 65536（BMP），对单个 Unicode codepoint 提供 O(1) 的 pair_index+1（0 表示无映射）。
  - `zh_conv_embedded_lookup` 接受 UTF-8 字符串 `key`，会先尝试单字符快查（若输入为单个 codepoint 且位于 BMP），否则在对应索引数组上做二分查找并返回匹配的 C 字符串指针（返回值为 `const char*`，指向生成文件中的常量字符串，调用方无需 free）。

- **在 C++ 中的使用示例**：

  - 简单的单字符转换示例（繁->简）：

    ```cpp
    #include "text/zh_conv_table_generated.cpp" // 或包含对应头/生成后的文件路径

    const char* out = zh_conv_embedded_lookup("漢", 1); // mode=1: trad->simp
    if (out) {
        Serial.printf("简体: %s\n", out);
    }
    ```

  - 将整段 UTF-8 文本逐字（或逐可能的短串）转换为目标（示例为繁->简）：

    ```cpp
    std::string convert_trad_to_simp(const std::string &in) {
        std::string out;
        // 简单策略：按 UTF-8 解码单个 codepoint，然后查表
        for (size_t i = 0; i < in.size();) {
            unsigned char c = (unsigned char)in[i];
            size_t clen = 1;
            if ((c & 0x80) == 0) clen = 1;
            else if ((c & 0xE0) == 0xC0) clen = 2;
            else if ((c & 0xF0) == 0xE0) clen = 3;
            else if ((c & 0xF8) == 0xF0) clen = 4;

            std::string key = in.substr(i, clen);
            const char* mapped = zh_conv_embedded_lookup(key.c_str(), 1);
            if (mapped) out.append(mapped);
            else out.append(key);

            i += clen;
        }
        return out;
    }
    ```

  - 若需要支持短语级别的替换（多个字组合成映射），可在 `zh_conv_pairs` 上做更复杂的前缀匹配或维护额外的短语索引，脚本当前以单字符串对为主，且对多字符键支持二分查找（通过 `strcmp` 比较完整字符串）。

- **性能与体积考虑**：
  - 完整表可能较大，`zh_single_index` 本身占用 256KB（65536 * 4 bytes）；脚本在生成时确实会将整个数组写入源码 —— 这会显著增加编译后的代码/Flash 占用。请务必检查生成文件大小并在必要时采取以下措施：
    - 只生成 `zh_single_index` 的子集或移除单字快速表（修改脚本以生成更小的快速表），
    - 仅包含常用映射（通过过滤 CSV 或使用 `--simple`/自定义策略），
    - 在运行时采用哈希表或基于 trie 的压缩表以减小存储（但会增加运行时开销）。

- **集成建议**：
  - 把 `tools/gen_zh_table.py` 的运行加入到构建前任务（或 CI），并把生成文件 `src/text/zh_conv_table_generated.cpp` 添加到构建系统。
  - 如果希望避免把长数组直接放入源码，可修改脚本改为生成二进制 lookup 文件并在运行时以只读方式从 SPIFFS/PROGMEM 加载，或在编译时转换为 `.rodata` 段外的二进制资源。

  - **字符集范围**
  


**字符集范围总结**
- **源码文件**: generateCharset.js — 导出 `buildCharset({includeGBK, includeTraditional, includeJapanese})`，返回排序且去重的 `Uint32Array`。
- **ASCII（基础）**: 包含可打印 ASCII：0x20–0x7E（空格到 ~）。
- **GBK（简体/通用）**:
  - 优先：若环境支持 `TextDecoder('gbk')`，按字节对枚举并解码（lead 0x81–0xFE，trail 两段 0x40–0x7E 和 0x80–0xFE），将解码出的非空白字符加入集合（分块解码以控制内存）。
  - 回退：无 `gbk` 支持时，近似地包含 CJK 基本块：0x4E00–0x9FFF 和 扩展A 0x3400–0x4DBF。
- **Big5（繁体）**:
  - 优先：若支持 `TextDecoder('big5')`，按 Big5 编码枚举并解码（lead 0xA1–0xFE，trail 0x40–0x7E 和 0xA1–0xFE），加入解码得到的字符。
  - 回退：包含 0x4E00–0x9FFF、0x3400–0x4DBF 以及 0xF900–0xFAFF（兼容/繁体相关区块）。
- **日文（可选）**: 若 `includeJapanese` 为真，显式加入常用日文/相关块：
  - 平假名 0x3040–0x309F
  - 片假名 0x30A0–0x30FF
  - 半角片假名 0xFF65–0xFF9F
  - Katakana 扩展 0x31F0–0x31FF
  - CJK 汉字 0x4E00–0x9FAF 与 扩展A 0x3400–0x4DBF
  - 其它兼容/圈字符区 0x3200–0x32FF、0x3300–0x33FF
- **特殊字符**: 明确加入 U+2022、U+25A1、U+FEFF（BOM）三项以保证必要符号存在。
- **过滤与去重**:
  - 解码时会忽略空白字符（用 `/\s/` 过滤）和解码错误。
  - 最后以 Set 去重并排序，返回 `Uint32Array`。
- **参数控制**: 三个布尔参数控制是否包含 GBK、Big5（繁体）、日文分块，便于在不同固件/渲染策略下调整。

