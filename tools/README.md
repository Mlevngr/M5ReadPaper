# ReaderPaper 字体生成工具集

专为电子墨水屏优化的1位字体生成器工具集，支持命令行和GUI界面，具备边缘平滑功能以减少毛刺效果。

## � 目录结构说明

### 核心功能文件

| 文件 | 用途 | 说明 |
|------|------|------|
| `generate_1bit_font_bin.py` | **主要字体生成器** ### 打包后的exe文件
- **单文件模式** (onefile): 
  - 文件: `dist/FontGenerator.exe`
  - 大小: 30-90MB（取决于优化级别）
  - 启动时间: 15-30秒（需要解压）
  - 适合: 简单分发
  
- **目录模式** (onedir): 
  - 目录: `dist/FontGenerator/`
  - 执行文件: `dist/FontGenerator/FontGenerator.exe`
  - 大小: 100-200MB（包含所有依赖）
  - 启动时间: 2-5秒（无需解压）
  - 适合: 日常使用
  
- **共同特点**:
  - 无需Python环境
  - 包含自定义图标
  - 可在任何Windows系统运行OTF字体转换为1位二进制格式，支持边缘平滑、ASCII/GBK字符集 |
| `font_generator_gui.py` | **GUI界面程序** | 基于PySide6的现代化图形界面，适合日常使用和打包为exe |
| `generate_gbk_table.py` | **GBK映射表生成器** | 生成GBK到Unicode的完整映射表，用于ESP32项目 |

### 自动化脚本

| 文件 | 用途 | 说明 |
|------|------|------|
| `setup.bat` | **环境初始化脚本** | 自动创建虚拟环境并安装所有依赖包 |
| `generate_font.bat` | **字体生成快捷脚本** | 一键生成字体文件的批处理脚本 |
| `build_exe.bat` | **GUI打包脚本(CMD版)** | 将GUI程序打包为独立exe文件，支持中文编码，包含图标 |
| `build_exe.ps1` | **GUI打包脚本(PowerShell版)** | PowerShell版本的打包脚本，原生UTF-8支持，包含图标 |
| `build_exe_simple.bat` | **简化打包脚本** | 避免中文字符问题的纯ASCII版本打包脚本 |
| `build_exe_optimized.bat` | **优化打包脚本** | 最小化exe文件大小的优化版本，排除更多不必要模块 |
| `build_exe_fast.bat` | **快速启动脚本** | 目录模式打包，启动速度优化（2-5秒启动） |
| `build_exe_balanced.bat` | **平衡优化脚本** | 可选单文件或目录模式，平衡大小和速度 |

### 配置和文档

| 文件 | 用途 | 说明 |
|------|------|------|
| `requirements.txt` | **依赖包列表** | 包含所有必需和可选依赖，支持GUI和打包功能 |
| `FontGenerator.spec` | **PyInstaller配置** | exe打包的详细配置文件 |
| `README.md` | **主要说明文档** | 本文件，工具集使用指南 |
| `README_GUI.md` | **GUI使用文档** | GUI界面的详细使用说明 |
| `1bit_font_bin_format.md` | **文件格式文档** | 详细说明生成的bin文件的二进制结构 |

### 参考和临时文件

| 文件/目录 | 用途 | 说明 |
|-----------|------|------|
| `bin_font_generator.py.ref` | **历史版本参考** | 旧版本的字体生成器，保留作为参考 |
| `build/` | **构建临时目录** | PyInstaller构建过程中的临时文件 |
| `dist/` | **打包输出目录** | 最终生成的exe文件存放位置 |
| `working/` | **工作目录** | 存放生成的字体文件，如lite.bin |
| `venv/` | **虚拟环境目录** | Python虚拟环境，由setup.bat创建 |

## �🚀 快速开始

### 1. 环境设置
```bash
# 运行设置脚本（Windows，推荐）
setup.bat

# 或手动设置
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

### 2. 生成字体

#### 方式一：使用GUI界面（推荐）
```bash
# 直接运行GUI
python font_generator_gui.py

# 或使用打包好的exe
FontGenerator.exe
```

#### 方式二：使用命令行
```bash
# 使用快捷脚本
generate_font.bat ChillHuoSong_F_Regular.otf lite.bin

# 或手动运行
venv\Scripts\activate
python generate_1bit_font_bin.py --size 32 --white 80 ChillHuoSong_F_Regular.otf lite.bin
```

## � 工具链使用流程

### 完整开发流程

1. **环境准备**
   ```bash
   setup.bat  # 初始化开发环境
   ```

2. **字体生成**
   ```bash
   # GUI方式（推荐新手）
   python font_generator_gui.py
   
   # 命令行方式（推荐批量处理）
   generate_font.bat 字体文件.otf 输出文件.bin
   ```

3. **GBK映射表生成**（ESP32项目需要）
   ```bash
   # 生成完整映射表
   python generate_gbk_table.py --full > ../src/text/gbk_unicode_data.cpp
   
   # 生成简化映射表
   python generate_gbk_table.py --simple > ../src/text/gbk_unicode_data.cpp
   ```

4. **GUI程序打包**（分发使用）
   ```bash
   # 快速启动版本（推荐，2-5秒启动）
   build_exe_fast.bat
   
   # 平衡优化版本（可选模式）
   build_exe_balanced.bat
   
   # 优化版本（最小文件大小，但启动较慢）
   build_exe_optimized.bat
   
   # 简化版本（避免编码问题）
   build_exe_simple.bat
   
   # PowerShell版本（支持中文）
   build_exe.ps1
   ```

## �📋 命令行参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--size` | 32 | 字体像素高度 (28-42) |
| `--white` | 80 | 白色阈值 (0-255，越高字体越细) |
| `--no-smooth` | - | 禁用边缘平滑（更锐利但可能有毛刺） |
| `--no-gbk` | - | 仅包含ASCII字符集 |
| `--export-charset` | webapp/extension/assets/charset_default.json | 可选：将构建的字符集以 JSON 导出，供 webapp 使用。传入空字符串可禁用导出 |

## ✨ 核心特性

## Webapp 集成（确保 webapp 可访问导出的 charset JSON）

为了保证浏览器端与 Python 端使用完全一致的字符集，推荐在生成 .bin 时同时导出字符集 JSON，并把它放到 webapp 能访问的静态资源目录。默认行为：

- 在生成时使用参数 --export-charset 指定输出路径（默认：`webapp/extension/assets/charset_default.json`）。
- `main.js` 已实现多路径加载候选（例如 `../assets/charset_default.json`, `./assets/charset_default.json`, `/webapp/extension/assets/charset_default.json` 等），以提高不同开发或部署布局下的兼容性。

示例（建议，在项目根执行）：

```powershell
python tools/generate_1bit_font_bin.py --size 32 --white 80 ChillHuoSong_F_Regular.otf lite.bin --export-charset webapp/extension/assets/charset_default.json
```

部署说明：
- 开发时：将 JSON 放在 `webapp/extension/assets/`（或 `webapp/extension/pages` 可访问的任何位置），以便页面通过 `fetch()` 读取。
- 打包/发布时：把该文件包含进静态资源（或将其复制到静态服务器的合适路径）。如果你的静态服务器使用不同的根路径，请调整 `webapp/extension/pages/main.js` 中的候选加载路径，或者把 JSON 放到静态根的 `assets/` 下。

备选方式：如果你无法在构建阶段生成该文件，可以本地运行：

```powershell
python tools/generate_charset_json.py --out webapp/extension/assets/charset_default.json
```

注意：即使浏览器支持 TextDecoder('gbk'/'big5')，为了稳定一致性（尤其是在不同宿主环境/扩展宿主中），仍推荐将 JSON 打包到 webapp 中。


### 边缘平滑技术
默认启用边缘平滑处理，可以显著减少字体毛刺：

```bash
# 开启边缘平滑（默认，推荐）
python generate_1bit_font_bin.py font.otf output.bin

# 关闭边缘平滑（更锐利）
python generate_1bit_font_bin.py --no-smooth font.otf output.bin
```

## 🔧 依赖说明

### 字符集支持
- **ASCII字符集**: 基本英文字符和符号
- **GBK字符集**: 完整的中文字符支持（约21000个字符）
- **自定义字符集**: 可通过修改代码支持其他字符集

### 1位二进制格式
- 专为电子墨水屏等二值显示设备优化
- 紧凑的数据结构，节省存储空间
- 详细格式说明见 `1bit_font_bin_format.md`

## 🔧 依赖管理

### 必需依赖
- `numpy>=1.20.0` - 数值计算和图像处理
- `freetype-py>=2.3.0` - 字体文件解析和渲染

### 可选依赖
- `scipy>=1.7.0` - 高质量边缘平滑算法（推荐安装）
- `PySide6>=6.4.0` - GUI界面支持
- `pyinstaller>=5.0` - exe打包工具

### 安装方式
```bash
# 安装所有依赖（推荐）
pip install -r requirements.txt

# 仅安装核心依赖
pip install numpy freetype-py

# 添加GUI支持
pip install PySide6

# 添加打包支持
pip install pyinstaller
```

## 📁 输出文件说明

### 生成的字体文件
- **lite.bin**: 标准输出的字体文件，包含指定字符集
- **working/**: 工作目录，存放生成的字体文件
- **详细格式**: 参考 `1bit_font_bin_format.md`

### 打包后的exe文件
- **dist/FontGenerator.exe**: 独立运行的GUI程序，包含自定义图标
- **无需Python环境**: 可在任何Windows系统运行
- **文件大小**: 
  - 标准打包: 约80-90MB
  - 优化打包: 约30-50MB（使用build_exe_optimized.bat）
- **图标支持**: 使用tools/icon.ico作为exe图标

## 🛠️ 开发和调试

### 测试字体生成
```bash
# 使用示例字体测试
generate_font.bat example.otf test.bin

# 检查生成的文件
ls -la working/
```

### GUI开发
```bash
# 直接运行开发版本
python font_generator_gui.py

# 打包测试版本
build_exe.bat
```

### GBK映射表更新
```bash
# 生成新的映射表
python generate_gbk_table.py --full > ../src/text/gbk_unicode_data.cpp

# 验证映射表
python generate_gbk_table.py --test
```

## 📖 相关文档

- **GUI使用指南**: `README_GUI.md`
- **文件格式规范**: `1bit_font_bin_format.md`
- **项目架构文档**: `../docs/task_system_architecture.md`
- **GBK字符统计**: `../docs/GBK_CHARACTER_COUNT.md`

## 🤝 贡献指南

1. 确保代码通过测试
2. 更新相关文档
3. 遵循现有代码风格
4. 提交前运行完整测试流程

## 📄 许可证

本项目采用开源许可证，具体条款请参考项目根目录的LICENSE文件。
- `scipy` - 高质量边缘平滑（强烈推荐）
  - 如果未安装会自动回退到简化算法

## 📊 最佳实践

根据测试，以下设置效果最佳：
- **字体大小**: 32px
- **白色阈值**: 80
- **边缘平滑**: 开启

这些设置能在保持清晰度的同时最大程度减少毛刺。

## 🎯 使用示例

```bash
# 标准中文字体（推荐设置）
python generate_1bit_font_bin.py --size 32 --white 80 SimSun.ttf simsun.bin

# 更细的字体
python generate_1bit_font_bin.py --size 32 --white 60 font.otf thin.bin

# 仅英文字符集（更小文件）
python generate_1bit_font_bin.py --no-gbk --size 28 arial.ttf arial_en.bin

# 禁用平滑（更锐利边缘）
python generate_1bit_font_bin.py --no-smooth --size 32 font.otf sharp.bin
```

## �️ Demo 图片预览

脚本支持直接生成一张用于预览的 PNG 图片，便于在主机上快速查看生成的二值化渲染效果（白底黑字、无边框，周围保留一个字符高度的 margin，字符间会插入一个空格宽度）。

新增参数：
- `--demo <text|@file>`: 如果传入普通字符串，脚本会渲染该字符串；如果以 `@` 开头，例如 `@chars.txt`，脚本会读取文件内容并渲染。
- `--demo-out <path>`: 指定 demo PNG 的输出路径（默认 `demo.png`）。
- `--demo-scale <int>`: demo 图片缩放倍数，整数，默认 `2`（方便观察像素效果）。

示例：
```bash
# 直接渲染字符串并生成 demo.png
python generate_1bit_font_bin.py --size 32 --white 80 font.otf out.bin --demo "示例文本"

# 使用文件中的字符并自定义输出/缩放
python generate_1bit_font_bin.py font.otf out.bin --demo @chars.txt --demo-out my_demo.png --demo-scale 3
```

## �📁 输出文件

生成的`.bin`文件包含：
- 文件头信息
- 字符索引表
- 1位压缩位图数据

文件大小通常比原字体文件小90%以上。
