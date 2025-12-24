#pragma once

#include <string>
#include <vector>
#include <cstddef>

// 简单的书签标签条目
struct TagEntry
{
    size_t position;       // 文件内字节偏移
    std::string preview;   // 从该位置起的前10个非空字符（UTF-8友好）
    float percentage;      // position / total_length * 100.0
    bool is_auto = false;  // 若为 true 则表示这是自动生成的 slot0 标签
};

// 读取同名书籍的 .tags 文件，若文件不存在返回空向量
std::vector<TagEntry> loadTagsForFile(const std::string &book_file_path);

// 在 .tags 中插入一条（若已存在相同位置则更新），按 position 升序排序，最多保留 10 条
// 返回是否成功写入到 SD
bool insertTagForFile(const std::string &book_file_path, size_t position);
// 插入 tag 并使用外部提供的预览字符串（避免再次从文件读取）。
bool insertTagForFile(const std::string &book_file_path, size_t position, const std::string &preview_override);

// 插入/更新自动 slot0 标签（覆盖 slot0）。自动标签与手动标签分开管理：
// 自动标签位于返回的 tags 向量的索引0（若存在），手动标签使用 slot1-slot9。
bool insertAutoTagForFile(const std::string &book_file_path, size_t position);
bool insertAutoTagForFile(const std::string &book_file_path, size_t position, const std::string &preview_override);

// 按位置删除匹配的 tag（exact match），返回是否有改动并成功写入
bool deleteTagForFileByPosition(const std::string &book_file_path, size_t position);

// 按索引删除（0-based，按文件中当前顺序），返回是否有改动并成功写入
bool deleteTagForFileByIndex(const std::string &book_file_path, size_t index);

// 删除同名 .tags 文件（清空全部）
bool clearTagsForFile(const std::string &book_file_path);
