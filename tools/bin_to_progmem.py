#!/usr/bin/env python3
"""
bin_to_progmem.py - 将 .bin 字体文件转换为 C 代码（PROGMEM）
=============================================================

此脚本将生成的 .bin 字体文件转换为可编译进 ESP32 Flash 的 C 代码。
生成的代码使用 PROGMEM 宏，确保数据存储在 Flash 而不是 RAM 中。

使用方法:
  python bin_to_progmem.py font.bin -o src/text/progmem_font_data.cpp

选项:
  --variable-name NAME    自定义变量名前缀（默认：progmem_font）
  --chunk-size SIZE       每行输出的字节数（默认：16）
  --add-stats            添加详细的统计信息到输出文件
"""

import argparse
import os
import sys
import struct


def read_bin_font_info(bin_path):
    """读取 .bin 字体文件的头部信息"""
    with open(bin_path, 'rb') as f:
        # 读取头部（134字节）
        char_count = struct.unpack('<I', f.read(4))[0]
        font_height = struct.unpack('<B', f.read(1))[0]
        version = struct.unpack('<B', f.read(1))[0]
        family_name = f.read(64).rstrip(b'\0').decode('utf-8', errors='ignore')
        style_name = f.read(64).rstrip(b'\0').decode('utf-8', errors='ignore')
        
        # 获取文件大小
        f.seek(0, 2)  # 移动到文件末尾
        file_size = f.tell()
        
    return {
        'char_count': char_count,
        'font_height': font_height,
        'version': version,
        'family_name': family_name,
        'style_name': style_name,
        'file_size': file_size
    }


def generate_progmem_cpp(bin_path, output_path, variable_name='progmem_font', chunk_size=16, add_stats=False):
    """
    将 .bin 文件转换为 C++ 代码（PROGMEM）
    
    Args:
        bin_path: 输入的 .bin 字体文件路径
        output_path: 输出的 .cpp 文件路径  
        variable_name: 变量名前缀
        chunk_size: 每行输出的字节数
        add_stats: 是否添加详细统计信息
    """
    
    # 读取字体信息
    info = read_bin_font_info(bin_path)
    
    # 读取完整的二进制数据
    with open(bin_path, 'rb') as f:
        font_data = f.read()
    
    # 生成 C++ 文件
    with open(output_path, 'w', encoding='utf-8') as out:
        # 文件头注释
        out.write('// 自动生成的 PROGMEM 字体数据文件\n')
        out.write(f'// 源文件: {os.path.basename(bin_path)}\n')
        out.write(f'// 字体: {info["family_name"]} {info["style_name"]}\n')
        out.write(f'// 大小: {info["font_height"]}px\n')
        out.write(f'// 字符数: {info["char_count"]}\n')
        out.write(f'// 文件大小: {info["file_size"]} 字节 ({info["file_size"]/1024:.2f} KB)\n')
        out.write('// \n')
        out.write('// 警告：此文件由脚本自动生成，请勿手动编辑！\n')
        out.write('// 生成命令: python bin_to_progmem.py ' + os.path.basename(bin_path) + '\n')
        out.write('\n')
        
        # 包含头文件
        out.write('#define PROGMEM_FONT_DATA_IMPL\n')
        out.write('#include "progmem_font_data.h"\n')
        out.write('\n')
        
        # 全局标志和大小
        out.write('// 全局标志：PROGMEM 字体数据可用\n')
        out.write('const bool g_has_progmem_font = true;\n')
        out.write('\n')
        out.write('// 字体数据总大小\n')
        out.write(f'const uint32_t g_progmem_font_size = {len(font_data)};\n')
        out.write('\n')
        
        # 字体数据数组
        out.write('// 字体数据（存储在 Flash）\n')
        out.write(f'const uint8_t g_{variable_name}_data[] PROGMEM = {{\n')
        
        # 分块写入数据
        for i in range(0, len(font_data), chunk_size):
            chunk = font_data[i:i+chunk_size]
            hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
            
            # 添加注释标记位置
            if i % (chunk_size * 10) == 0:
                out.write(f'    // Offset: 0x{i:06X} ({i})\n')
            
            out.write(f'    {hex_values}')
            
            # 除了最后一行，都要加逗号
            if i + chunk_size < len(font_data):
                out.write(',')
            
            out.write('\n')
        
        out.write('};\n')
        out.write('\n')
        
        # 别名（指向数据数组）
        out.write('// 别名：方便外部访问\n')
        out.write(f'const uint8_t* const g_progmem_font_data = g_{variable_name}_data;\n')
        out.write('\n')
        
        # 添加统计信息（可选）
        if add_stats:
            out.write('// ========== 详细统计信息 ==========\n')
            out.write('// \n')
            out.write(f'// 字体族名: {info["family_name"]}\n')
            out.write(f'// 字体样式: {info["style_name"]}\n')
            out.write(f'// 字体高度: {info["font_height"]} 像素\n')
            out.write(f'// 格式版本: {info["version"]}\n')
            out.write(f'// 字符总数: {info["char_count"]}\n')
            out.write('// \n')
            out.write(f'// 头部大小: 134 字节\n')
            out.write(f'// 索引表大小: {info["char_count"] * 20} 字节\n')
            out.write(f'// 位图数据: {info["file_size"] - 134 - info["char_count"] * 20} 字节\n')
            out.write('// \n')
            out.write(f'// Flash 占用: {info["file_size"]} 字节 ({info["file_size"]/1024:.2f} KB)\n')
            out.write('// RAM 占用: 0 字节（全部存储在 Flash）\n')
            out.write('// \n')
            out.write('// ====================================\n')
    
    return info


def main():
    parser = argparse.ArgumentParser(
        description='将 .bin 字体文件转换为 C/C++ PROGMEM 代码',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 基本用法
  python bin_to_progmem.py font.bin -o src/text/progmem_font_data.cpp
  
  # 自定义变量名和输出格式
  python bin_to_progmem.py font.bin -o output.cpp --variable-name my_font --chunk-size 12
  
  # 包含详细统计信息
  python bin_to_progmem.py font.bin -o output.cpp --add-stats
        """
    )
    
    parser.add_argument('input', help='输入的 .bin 字体文件')
    parser.add_argument('-o', '--output', required=True, help='输出的 .cpp 文件路径')
    parser.add_argument('--variable-name', default='progmem_font',
                       help='变量名前缀（默认: progmem_font）')
    parser.add_argument('--chunk-size', type=int, default=16,
                       help='每行输出的字节数（默认: 16）')
    parser.add_argument('--add-stats', action='store_true',
                       help='添加详细的统计信息到输出文件')
    
    args = parser.parse_args()
    
    # 验证输入文件
    if not os.path.isfile(args.input):
        print(f'❌ 错误: 输入文件不存在: {args.input}')
        return 1
    
    # 验证文件扩展名
    if not args.input.endswith('.bin'):
        print(f'⚠️  警告: 输入文件不是 .bin 文件: {args.input}')
    
    # 确保输出目录存在
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 生成 C++ 代码
    print(f'🔧 正在转换字体文件...')
    print(f'📄 输入: {args.input}')
    print(f'📁 输出: {args.output}')
    
    try:
        info = generate_progmem_cpp(
            args.input,
            args.output,
            variable_name=args.variable_name,
            chunk_size=args.chunk_size,
            add_stats=args.add_stats
        )
        
        print(f'\n✅ 转换成功！')
        print(f'\n字体信息:')
        print(f'  字体族名: {info["family_name"]}')
        print(f'  字体样式: {info["style_name"]}')
        print(f'  字体大小: {info["font_height"]}px')
        print(f'  字符数量: {info["char_count"]}')
        print(f'  文件大小: {info["file_size"]} 字节 ({info["file_size"]/1024:.2f} KB)')
        print(f'\n生成的 C++ 文件包含 {len(open(args.output).readlines())} 行代码')
        print(f'Flash 占用: {info["file_size"]} 字节')
        print(f'RAM 占用: 0 字节（使用 PROGMEM）')
        
        return 0
        
    except Exception as e:
        print(f'\n❌ 转换失败: {e}')
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
