#pragma once
#include <vector>
#include <string>
#include <functional>

struct UIFileItem {
    std::string filename;
    bool selected = false;
    
    UIFileItem(const std::string& name) : filename(name) {}
    UIFileItem() = default;
};

struct UIButton {
    std::string text;
    std::function<void(const UIFileItem&)> callback;
};

// 文件列表UI主函数（仅负责UI显示，不处理事件）
void ui_show_file_list(const std::vector<UIFileItem>& files,
                      const std::vector<UIButton>& buttons,
                      int page_size = 8);
