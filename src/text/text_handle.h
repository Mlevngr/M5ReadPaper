#ifndef TEXT_HANDLE_H
#define TEXT_HANDLE_H

#include <string>
#include <vector>
#include <cstdint>
#include <FS.h>
// forward-declare BookHandle to allow build_book_page_index to query stop requests
class BookHandle;

// 支持的文本编码类型
enum class TextEncoding {
    UTF8,
    GBK,
    AUTO_DETECT  // 自动检测
};

struct TextState {
    std::string file_path;
    size_t file_pos;
    size_t page_end_pos;
    std::string last_page;
    TextEncoding encoding;  // 当前文件的编码
    size_t prev_page_start = 0; // 缓存上一页的起点（加速向前翻页），0表示未知或文件开始
};


struct TextPageResult {
    bool success;
    size_t file_pos;      // 本页起始位置
    size_t page_end_pos;  // 本页结束位置（下次翻页的起点）
    std::string page_text;
};

// 分页结果结构
struct PageBreakResult {
    std::vector<size_t> line_breaks;  // 每行的断点位置
    size_t page_end_pos;              // 页面结束位置
    int lines_count;                  // 行数
    bool success;                     // 是否成功
    
    PageBreakResult() : page_end_pos(0), lines_count(0), success(false) {}
};

// 统一的分页函数：计算给定文本段的分页信息
// 这个函数被 read_text_page 和 build_book_page_index 共同使用
PageBreakResult calculate_page_breaks(const std::string& text, size_t start_pos, 
                                    int16_t area_width, int16_t area_height, 
                                    float font_size, int max_lines, int16_t max_width, 
                                    bool vertical = false);

// 仅支持：接受已经打开的文件句柄（caller 负责 open/close），避免频繁 open/close
// backward=false：从 start_pos 向后读取（下一页）；
// backward=true：尝试读取 start_pos 之前的一页（上一页）。若前面内容不足一页，返回从0开始的一页。
// max_byte_pos: 如果指定（!= SIZE_MAX），读取不会超过此位置（用于限制在页面边界内）
TextPageResult read_text_page(File &file, const std::string& file_path, size_t start_pos,
    int16_t area_width, int16_t area_height, float font_size,
    TextEncoding encoding = TextEncoding::AUTO_DETECT, bool backward = false, bool vertical = false, size_t max_byte_pos = SIZE_MAX);

// 快速索引：生成整书的页面起始位置（每页 start_pos）。
// 返回结构包含页面列表以及本次生成是否到达文件末尾（reached_eof）。
struct BuildIndexResult {
    std::vector<size_t> pages;
    bool reached_eof;
    BuildIndexResult() : reached_eof(false) {}
};

// - file: 已打开的文件句柄（caller 负责 open/close）
// - max_pages: 若>0，则最多返回该数量的 page positions（用于增量生成）
// - start_offset: 从文件的该原始字节偏移开始生成索引（默认0）
BuildIndexResult build_book_page_index(File &file, const std::string &file_path,
                                       int16_t area_width, int16_t area_height, float font_size,
                                       TextEncoding encoding = TextEncoding::AUTO_DETECT,
                                       size_t max_pages = 0, size_t start_offset = 0, bool vertical = false,
                                       BookHandle* bh = nullptr);

// 编码检测和转换函数
TextEncoding detect_text_encoding(const uint8_t* buffer, size_t size);
std::string convert_to_utf8(const std::string& input, TextEncoding from_encoding);

// 全局状态寄存
extern TextState g_text_state;

#endif // TEXT_HANDLE_H
