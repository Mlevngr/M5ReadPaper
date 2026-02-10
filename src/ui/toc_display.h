#pragma once

#include <M5Unified.h>
#include <vector>
#include <string>

// TOC (Table of Contents) entry structure parsed from .idx file
// Format: #序号#, #标题#, #字节位置#, #百分比#,
struct TocEntry
{
    int index;           // 序号
    std::string title;   // 标题
    size_t position;     // 字节位置
    float percentage;    // 百分比

    TocEntry() : index(0), position(0), percentage(0.0f) {}
};

// Retrieve a single entry by its absolute index in the TOC (0-based).
// Returns true when the entry exists and is parsed successfully.
bool fetch_toc_entry(const std::string &book_file_path, size_t toc_index, TocEntry &entry);

// Draw a left-side 450x960 TOC list, 10 rows (each ~86px high).
// Each row shows: <标题> <百分比>
// Similar to show_tag_ui but displays TOC entries instead of tags
// paging : 0 - 全屏刷， 1-刷列表条目部分（翻页） 2- 刷列表部分（用于书签目录跳转）
void show_toc_ui(M5Canvas *canvas = nullptr, int8_t  paging = 0);

// Warm up TOC cache so the first UI entry is faster
void toc_prefetch_for_book(const std::string &book_file_path);

// Pagination control for TOC display
// These functions change or reset the current TOC page used by show_toc_ui.
void toc_next_page();
void toc_prev_page();
void toc_reset_page();
// Get current page offset for touch handling
int toc_get_current_page();
// Jump to the TOC page that contains the entry closest to the given file position
void toc_jump_to_position(const std::string &book_file_path, size_t file_pos);

// Clear the cached TOC page so next render re-reads from storage
void toc_invalidate_page_cache();

// When set to true the next `show_toc_ui` call should refresh its view
// and re-evaluate the current book/page. This is set by state transition
// code when switching into the TOC display state.
extern bool toc_refresh;

// Find the TOC entry closest to and not greater than `file_pos`.
// Returns true if a candidate was found. Outputs:
//  - out_entry_index: absolute index in the TOC (0-based)
//  - out_page: page index that contains the entry (0-based)
//  - out_row_in_page: row index within that page (0-based, < TOC_ROWS)
//  - out_on_current_page: whether the entry is on the currently displayed page
bool find_toc_entry_for_position(const std::string &book_file_path, size_t file_pos,
                                 size_t &out_entry_index, int &out_page, int &out_row_in_page,
                                 bool &out_on_current_page);

// Get the title string for a given absolute TOC index (0-based).
// This will load at most one TOC page from the .idx file into the TOC page cache
// (fast; ~10 entries) and return the title. Returns true on success.
bool get_toc_title_for_index(const std::string &book_file_path, size_t toc_index, std::string &out_title);

// Start an asynchronous background load of the TOC page that contains `page_index`.
// This will populate `g_toc_cache.cached_entries` and set `g_toc_cache.cached_page` when done.
// Calling this from the UI avoids blocking file I/O on the main thread.
void start_async_load_toc_page(const std::string &book_file_path, int page_index);

// Last result recorded by `toc_jump_to_position`.
// If `toc_last_entry_valid` is true, the other three fields contain the
// absolute TOC entry index, page index and row-in-page (0-based).
extern size_t toc_last_entry_index;
extern int toc_last_entry_page;
extern int toc_last_entry_row;
extern bool toc_last_entry_valid;
