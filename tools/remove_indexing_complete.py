#!/usr/bin/env python3
"""
删除 BookHandle 中对 indexing_complete 标志的所有使用
实现"完全信任磁盘"策略
"""

import re

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original = content
    
    # 1. 构造函数中删除 indexing_complete(false) 初始化
    content = re.sub(
        r'indexing_complete\(false\),\s*',
        '',
        content
    )
    
    # 2. 删除所有 indexing_complete = xxx; 赋值语句（保留注释）
    content = re.sub(
        r'^\s*indexing_complete\s*=\s*[^;]+;\s*(?://.*)?$',
        '',
        content,
        flags=re.MULTILINE
    )
    
    # 3. 替换调试打印中的 indexing_complete ? "是" : "否" 为 isIndexingComplete() ? "YES" : "NO"
    content = re.sub(
        r'indexing_complete\s*\?\s*"是"\s*:\s*"否"',
        'isIndexingComplete() ? "YES" : "NO"',
        content
    )
    
    # 4. 替换调试打印中的英文版本
    content = re.sub(
        r'indexing_complete\s*\?\s*"true"\s*:\s*"false"',
        'isIndexingComplete() ? "true" : "false"',
        content
    )
    
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"✓ Updated {filepath}")
        return True
    else:
        print(f"- No changes needed in {filepath}")
        return False

if __name__ == '__main__':
    changed = process_file('c:/studio/ReaderPaper/src/text/book_handle.cpp')
    exit(0 if changed else 1)
