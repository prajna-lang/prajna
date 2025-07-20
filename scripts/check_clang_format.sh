#!/bin/bash

# 调试：打印当前工作目录
echo "当前工作目录: $(pwd)"

# 检查.clang-format文件是否存在
if [ -f .clang-format ]; then
    echo "找到.clang-format文件，将使用项目自定义格式配置。"
else
    echo "警告：未找到.clang-format文件，clang-format将使用默认样式。"
fi

# 调试：检查find命令是否找到文件，仅搜索prajna和tests目录
echo "正在查找prajna和tests目录中的.cpp、.h、.hpp、.cxx文件..."
FOUND_FILES=$(find prajna tests \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cxx" \) 2>/dev/null)
if [ -z "$FOUND_FILES" ]; then
    echo "在prajna和tests目录中未找到任何.cpp、.h、.hpp、.cxx文件，跳过clang-format检查。"
    exit 0
else
    echo "找到以下文件："
    # echo "$FOUND_FILES"
fi

# 创建临时文件存储clang-format输出
TEMP_FILE="clang_format_output.txt"
> "$TEMP_FILE"

# 运行clang-format检查，捕获stdout和stderr，仅检查prajna和tests目录
echo "运行clang-format检查..."
find prajna tests \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cxx" \) -exec clang-format --dry-run --verbose {} \; >> "$TEMP_FILE" 2>&1

# 提取不符合规范的文件
NON_COMPLIANT_FILES=$(grep "warning: code should be clang-formatted" "$TEMP_FILE" | cut -d: -f1 | sort | uniq)

# 调试：仅显示不符合规范文件的相关输出
echo "clang-format输出内容（仅显示不符合规范的文件）："
if [ -n "$NON_COMPLIANT_FILES" ]; then
    # 遍历每个不符合规范的文件，提取相关的Formatting和warning行
    echo "$NON_COMPLIANT_FILES" | while read -r file; do
        grep -B1 "^${file}:" "$TEMP_FILE" | grep -E "(Formatting \[1/1\] ${file}$|^${file}:.*warning: code should be clang-formatted)"
    done
else
    echo "无不符合规范的文件。"
fi

# 输出结果
if [ -n "$NON_COMPLIANT_FILES" ]; then
    echo "警告：以下文件不符合clang-format规范，请运行 'clang-format -i <file>' 修复："
    echo "$NON_COMPLIANT_FILES" | while read -r file; do
        echo "- $file"
    done
    rm -f "$TEMP_FILE"
    exit -1
else
    echo "所有文件均符合clang-format规范。"
fi

# 清理临时文件
rm -f "$TEMP_FILE"

# 始终返回0，确保构建不中断
exit 0
