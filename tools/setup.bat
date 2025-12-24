@echo off
rem 尝试使用 UTF-8 输出，以减少 CMD 中的中文/emoji 乱码问题
rem 注意：Windows 控制台在旧版中对 UTF-8 支持有限，推荐使用 Windows Terminal 或 PowerShell
chcp 65001 >nul 2>&1
echo (已切换到 UTF-8 输出, codepage 65001)
echo 🚀 设置字体生成工具环境...

REM 检查是否已存在虚拟环境
if exist "venv\" (
    echo ✅ 虚拟环境已存在
) else (
    echo 📦 创建虚拟环境...
    python -m venv venv
    if errorlevel 1 (
        echo ❌ 创建虚拟环境失败，请确保Python已安装
        pause
        exit /b 1
    )
)

echo 🔧 激活虚拟环境...
call venv\Scripts\activate.bat

echo 📥 安装依赖包...
pip install --upgrade pip
pip install -r requirements.txt

echo 🎉 设置完成！

echo.
echo 📋 使用方法:
echo   1. 激活环境: venv\Scripts\activate.bat
echo   2. 生成字体: python generate_1bit_font_bin.py --size 32 --white 80 font.otf output.bin
echo   3. 退出环境: deactivate
echo.

rem 提示：如果你仍然看到乱码，建议使用 PowerShell 或 Windows Terminal，并确保控制台字体设置为支持中文的等宽字体（如 "Consolas" 或 "Microsoft YaHei Mono"）
pause
