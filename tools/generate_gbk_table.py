#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GBK到Unicode查找表生成器
用于生成ESP32项目中使用的完整GBK映射表

用法:
python generate_gbk_table.py --full > ../src/text/gbk_unicode_data.cpp
python generate_gbk_table.py --simple > ../src/text/gbk_unicode_data.cpp

要求:
- Python 3.x
- 系统支持GBK编码
"""

import sys
import datetime

def log_message(msg):
    """输出日志信息到stderr，不影响重定向输出"""
    print(f"[LOG] {msg}", file=sys.stderr)

def generate_gbk_unicode_table():
    """生成完整的GBK到Unicode映射表"""
    log_message("开始生成完整GBK映射表...")
    
    print("// GBK到Unicode完整映射表数据文件")
    print("// 此文件由脚本自动生成，请勿手动编辑")
    print(f"// 生成时间: {datetime.datetime.now()}")
    print("// 表大小: 完整GBK字符集映射")
    print("")
    print("#include \"gbk_unicode_table.h\"")
    print("#include <Arduino.h>")
    print("")
    print("// 完整的GBK到Unicode映射表（存储在Flash PROGMEM中）")
    print("// 按GBK编码排序，支持二分查找")
    print("// 每个条目4字节：2字节GBK码 + 2字节Unicode码")
    print("const GBKToUnicodeEntry gbk_to_unicode_table[] PROGMEM = {")
    
    total_chars = 0
    
    # GBK完整编码范围按顺序处理：
    # 1. 8140-A0FE: GBK/1区，包含ASCII、图形符号等
    # 2. A1A1-A9FE: GBK/2区，符号区  
    # 3. AA40-A0FE: GBK/4区的前半部分
    # 4. B0A1-FAF9: GBK/3区，汉字区（一级汉字3755个，二级汉字3008个）
    # 5. FB40-FEFE: GBK/4区的后半部分
    
    # 1. GBK第1区：扩展ASCII和特殊符号 (8140-A0FE)
    log_message("处理GBK第1区 - 扩展ASCII区 (8140-A0FE)...")
    print("    // GBK第1区 - 扩展ASCII和符号 (8140-A0FE)")
    
    for high_byte in range(0x81, 0xA1):  # 81-A0
        if high_byte % 4 == 1:
            log_message(f"处理第1区高字节 0x{high_byte:02X}... (已处理 {total_chars} 个字符)")
            
        for low_byte in range(0x40, 0xFF):  # 40-FE，跳过7F
            if low_byte == 0x7F:  # 7F不是有效的GBK低字节
                continue
                
            try:
                gbk_code = (high_byte << 8) | low_byte
                gbk_bytes = bytes([high_byte, low_byte])
                
                try:
                    unicode_char = gbk_bytes.decode('gbk')
                    unicode_code = ord(unicode_char)
                    
                    # 生成安全的注释 - 避免控制台编码问题
                    try:
                        # 尝试编码为ASCII来检查是否安全
                        unicode_char.encode('ascii')
                        # 如果字符是可打印的ASCII字符，直接使用
                        if unicode_char.isprintable() and len(unicode_char) == 1:
                            comment = unicode_char
                        else:
                            comment = f"U+{unicode_code:04X}"
                    except UnicodeEncodeError:
                        # 非ASCII字符，使用Unicode码点表示
                        # 对于中文字符，添加字符类型信息
                        if 0x4E00 <= unicode_code <= 0x9FFF:  # CJK统一汉字
                            comment = f"U+{unicode_code:04X} CJK"
                        elif 0x3000 <= unicode_code <= 0x303F:  # CJK符号和标点
                            comment = f"U+{unicode_code:04X} CJK_PUNCT"
                        elif 0xFF00 <= unicode_code <= 0xFFEF:  # 全角ASCII
                            comment = f"U+{unicode_code:04X} FULLWIDTH"
                        else:
                            comment = f"U+{unicode_code:04X}"
                    
                    print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
                    total_chars += 1
                    
                except UnicodeDecodeError:
                    continue
                    
            except Exception:
                continue
    
    log_message(f"第1区完成，已处理 {total_chars} 个字符")
    
    # 2. GBK第2区：符号区 (A1A1-A9FE) 
    log_message("处理GBK第2区 - 符号区 (A1A1-A9FE)...")
    print("    // GBK第2区 - 符号区 (A1A1-A9FE)")
    
    for high_byte in range(0xA1, 0xAA):  # A1-A9
        for low_byte in range(0xA1, 0xFF):  # A1-FE
            try:
                gbk_code = (high_byte << 8) | low_byte
                gbk_bytes = bytes([high_byte, low_byte])
                
                try:
                    unicode_char = gbk_bytes.decode('gbk')
                    unicode_code = ord(unicode_char)
                    
                    # 生成安全的注释
                    try:
                        unicode_char.encode('ascii')
                        if unicode_char.isprintable() and len(unicode_char) == 1:
                            comment = unicode_char
                        else:
                            comment = f"U+{unicode_code:04X}"
                    except UnicodeEncodeError:
                        if 0x4E00 <= unicode_code <= 0x9FFF:
                            comment = f"U+{unicode_code:04X} CJK"
                        elif 0x3000 <= unicode_code <= 0x303F:
                            comment = f"U+{unicode_code:04X} CJK_PUNCT" 
                        elif 0xFF00 <= unicode_code <= 0xFFEF:
                            comment = f"U+{unicode_code:04X} FULLWIDTH"
                        else:
                            comment = f"U+{unicode_code:04X}"
                    
                    print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
                    total_chars += 1
                    
                except UnicodeDecodeError:
                    continue
                    
            except Exception:
                continue
    
    log_message(f"第2区完成，已处理 {total_chars} 个字符")
    
    # 3. GBK第4区前半部分：用户定义区 (AA40-A0FE)
    log_message("处理GBK第4区前半部分 - 用户定义区 (AA40-A0FE)...")
    print("    // GBK第4区前半部分 - 用户定义区 (AA40-A0FE)")
    
    # 正序处理 AA-A0
    for high_byte in range(0xAA, 0xA1):  # AA-A0
        if high_byte % 4 == 0:
            log_message(f"处理第4区前半部分高字节 0x{high_byte:02X}... (已处理 {total_chars} 个字符)")
            
        for low_byte in list(range(0x40, 0x7F)) + list(range(0x80, 0xFF)):  # 40-7E, 80-FE
            try:
                gbk_code = (high_byte << 8) | low_byte
                gbk_bytes = bytes([high_byte, low_byte])
                
                try:
                    unicode_char = gbk_bytes.decode('gbk')
                    unicode_code = ord(unicode_char)
                    
                    # 生成安全的注释
                    try:
                        unicode_char.encode('ascii')
                        comment = unicode_char if unicode_char.isprintable() and len(unicode_char) == 1 else f"U+{unicode_code:04X}"
                    except UnicodeEncodeError:
                        if 0x4E00 <= unicode_code <= 0x9FFF:
                            comment = f"U+{unicode_code:04X} CJK"
                        else:
                            comment = f"U+{unicode_code:04X}"
                    
                    print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
                    total_chars += 1
                    
                except UnicodeDecodeError:
                    continue
                    
            except Exception:
                continue
    
    log_message(f"第4区前半部分完成，已处理 {total_chars} 个字符")
    
    # 4. GBK第3区：汉字区 (B0A1-FAF9)
    log_message("处理GBK第3区 - 汉字区 (B0A1-FAF9)...")
    print("    // GBK第3区 - 汉字区 (B0A1-FAF9)")
    
    for high_byte in range(0xB0, 0xFB):  # B0-FA
        if high_byte % 8 == 0:  # 每8个高字节输出一次进度
            log_message(f"处理第3区高字节 0x{high_byte:02X}... (已处理 {total_chars} 个字符)")
            
        for low_byte in range(0xA1, 0xFF):  # A1-FE
            # FAF9 是GBK汉字区的结束
            if high_byte == 0xFA and low_byte > 0xF9:
                break
                
            try:
                gbk_code = (high_byte << 8) | low_byte
                gbk_bytes = bytes([high_byte, low_byte])
                
                try:
                    unicode_char = gbk_bytes.decode('gbk')
                    unicode_code = ord(unicode_char)
                    
                    # 汉字区 - 生成安全的注释
                    try:
                        unicode_char.encode('ascii')
                        comment = unicode_char if unicode_char.isprintable() and len(unicode_char) == 1 else f"U+{unicode_code:04X}"
                    except UnicodeEncodeError:
                        # 对于汉字，使用Unicode码点 + 类型标识
                        if 0x4E00 <= unicode_code <= 0x9FFF:
                            comment = f"U+{unicode_code:04X} CJK"
                        else:
                            comment = f"U+{unicode_code:04X}"
                    
                    print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
                    total_chars += 1
                    
                except UnicodeDecodeError:
                    continue
                    
            except Exception:
                continue
    
    log_message(f"第3区完成，已处理 {total_chars} 个字符")
    
    # 5. GBK第4区后半部分：用户定义区 (FB40-FEFE，包含F7E1)
    log_message("处理GBK第4区后半部分 - 用户定义区 (FB40-FEFE，包含F7E1)...")
    print("    // GBK第4区后半部分 - 用户定义区 (FB40-FEFE，包含F7E1)")
    
    # 处理 FB40-FEFE 范围 (这里包含了麼字：F7E1等重要字符)
    for high_byte in range(0xFB, 0xFF):  # FB-FE
        if high_byte % 2 == 1:
            log_message(f"处理第4区后半部分高字节 0x{high_byte:02X}... (已处理 {total_chars} 个字符)")
            
        for low_byte in list(range(0x40, 0x7F)) + list(range(0x80, 0xFF)):  # 40-7E, 80-FE
            try:
                gbk_code = (high_byte << 8) | low_byte
                gbk_bytes = bytes([high_byte, low_byte])
                
                try:
                    unicode_char = gbk_bytes.decode('gbk')
                    unicode_code = ord(unicode_char)
                    
                    # 生成安全的注释
                    try:
                        unicode_char.encode('ascii')
                        comment = unicode_char if unicode_char.isprintable() and len(unicode_char) == 1 else f"U+{unicode_code:04X}"
                    except UnicodeEncodeError:
                        if 0x4E00 <= unicode_code <= 0x9FFF:
                            comment = f"U+{unicode_code:04X} CJK"
                        else:
                            comment = f"U+{unicode_code:04X}"
                    
                    print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
                    total_chars += 1
                    
                except UnicodeDecodeError:
                    continue
                    
            except Exception:
                continue
    
    log_message(f"第4区后半部分完成，已处理 {total_chars} 个字符")
    
    print("};")
    print("")
    print(f"const size_t GBK_TABLE_SIZE = sizeof(gbk_to_unicode_table) / sizeof(GBKToUnicodeEntry);")
    print(f"// 总字符数: {total_chars}")
    
    log_message(f"生成完成！总共包含 {total_chars} 个字符映射")
    log_message(f"预估Flash占用: {total_chars * 4} 字节 ({total_chars * 4 / 1024:.1f}KB)")
    
    # 分析字符分布
    log_message("=== 字符分布分析 ===")
    log_message("理论GBK字符数：")
    log_message("- 第1区(8140-A0FE): 约6080个位置")
    log_message("- 第2区(A1A1-A9FE): 约846个位置") 
    log_message("- 第3区(B0A1-FAF9): 约6763个位置")
    log_message("- 理论总数: 约13689个位置")
    log_message(f"- 实际生成: {total_chars}个字符")
    log_message(f"- 覆盖率: {total_chars/13689*100:.1f}%")

def generate_simple_table():
    """生成简化的常用字符映射表（用于快速测试）"""
    
    # 常用汉字和标点的手动映射
    simple_mappings = [
        # 标点符号
        (0xA1A1, 0x3000, "全角空格"),
        (0xA1A2, 0x3001, "顿号"),
        (0xA1A3, 0x3002, "句号"),
        (0xA1A4, 0xFF1B, "分号"),
        (0xA1A5, 0xFF1A, "冒号"),
        (0xA1A6, 0xFF1F, "问号"),
        (0xA1A7, 0xFF01, "感叹号"),
        (0xA1A8, 0xFF0C, "逗号"),
        
        # 数字
        (0xA3B0, 0xFF10, "全角0"),
        (0xA3B1, 0xFF11, "全角1"),
        (0xA3B2, 0xFF12, "全角2"),
        (0xA3B3, 0xFF13, "全角3"),
        (0xA3B4, 0xFF14, "全角4"),
        (0xA3B5, 0xFF15, "全角5"),
        (0xA3B6, 0xFF16, "全角6"),
        (0xA3B7, 0xFF17, "全角7"),
        (0xA3B8, 0xFF18, "全角8"),
        (0xA3B9, 0xFF19, "全角9"),
        
        # 汉字数字
        (0xB2BB, 0x4E00, "一"),
        (0xB6FE, 0x4E8C, "二"),
        (0xC8FD, 0x4E09, "三"),
        (0xCBD4, 0x56DB, "四"),
        (0xCEE5, 0x4E94, "五"),
        (0xC1F9, 0x516D, "六"),
        (0xC6DF, 0x4E03, "七"),
        (0xB0CB, 0x516B, "八"),
        (0xBEF0, 0x4E5D, "九"),
        (0xCAB1, 0x5341, "十"),
        
        # 常用字
        (0xC4E3, 0x4F60, "你"),
        (0xBCD2, 0x597D, "好"),
        (0xCED2, 0x6211, "我"),
        (0xCBFB, 0x4ED6, "他"),
        (0xCBFD, 0x5979, "她"),
        (0xD5E2, 0x8FD9, "这"),
        (0xC4C7, 0x90A3, "那"),
        (0xC0B4, 0x6765, "来"),
        (0xC8A5, 0x53BB, "去"),
        (0xB5C4, 0x7684, "的"),
        (0xC1CB, 0x4E86, "了"),
        (0xD4DA, 0x5728, "在"),
        (0xD3D0, 0x6709, "有"),
        (0xCAC7, 0x662F, "是"),
        (0xB2BB, 0x4E0D, "不"),
        
        # 更多常用字
        (0xB0A1, 0x554A, "啊"),
        (0xCECA, 0x4E3A, "为"),
        (0xD2B2, 0x4E5F, "也"),
        (0xBBE1, 0x4F1A, "会"),
        (0xD2D1, 0x5DF2, "已"),
        (0xB6BC, 0x90FD, "都"),
        (0xC9CF, 0x4E0A, "上"),
        (0xCFC2, 0x4E0B, "下"),
        (0xB5BD, 0x5230, "到"),
        (0xB3F6, 0x51FA, "出"),
    ]
    
    print("// GBK到Unicode简化映射表数据文件")
    print("// 包含常用汉字和标点符号，适用于基本文本显示")
    print(f"// 生成时间: {datetime.datetime.now()}")
    print(f"// 字符数量: {len(simple_mappings)}")
    print("")
    print("#include \"gbk_unicode_table.h\"")
    print("#include <Arduino.h>")
    print("")
    print("const GBKToUnicodeEntry gbk_to_unicode_table[] PROGMEM = {")
    
    # 按GBK编码排序
    simple_mappings.sort(key=lambda x: x[0])
    
    for gbk_code, unicode_code, comment in simple_mappings:
        print(f"    {{0x{gbk_code:04X}, 0x{unicode_code:04X}}}, // {comment}")
    
    print("};")
    print("")
    print(f"const size_t GBK_TABLE_SIZE = {len(simple_mappings)};")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg == "--simple":
            log_message("生成简化映射表...")
            generate_simple_table()
        elif arg == "--full":
            log_message("生成完整映射表...")
            generate_gbk_unicode_table()
        elif arg == "--help":
            print("GBK到Unicode查找表生成器")
            print("")
            print("用法:")
            print("  python generate_gbk_table.py --simple   # 生成简化表(约60字符)")
            print("  python generate_gbk_table.py --full     # 生成完整表(约21000字符)")
            print("  python generate_gbk_table.py --help     # 显示帮助")
            print("")
            print("输出重定向:")
            print("  python generate_gbk_table.py --full > ../src/text/gbk_unicode_data.cpp")
            sys.exit(0)
        else:
            log_message(f"未知参数: {arg}")
            log_message("使用 --help 查看帮助")
            sys.exit(1)
    else:
        log_message("默认生成完整映射表...")
        log_message("使用 --help 查看更多选项")
        generate_gbk_unicode_table()
