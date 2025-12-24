# text_handle

用于处理txt文本文件分页显示。

## 功能
- 读取指定txt文件，从指定位置开始，截取一页内容（可配置最大字符数）
- 返回本页内容和结束位置
- 全局状态寄存：文件路径、当前指针、页尾指针、最后一页内容

## 用法示例
```cpp
#include "text_handle.h"
#include <SPIFFS.h>
// 打开文件并使用新的 File& 接口
File f = SPIFFS.open("/data/demo.txt", "r");
if (f) {
	TextPageResult res = read_text_page(f, std::string("/data/demo.txt"), 0, 800, 600, 16.0f, TextEncoding::AUTO_DETECT, false);
	display_print(res.page_text.c_str(), ...);
	f.close();
}
// 更新g_text_state 在函数内部处理
```
