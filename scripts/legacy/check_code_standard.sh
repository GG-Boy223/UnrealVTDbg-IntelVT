#!/bin/bash

# 项目规范检查脚本
# 检查所有源文件是否符合300行规则

echo "=== UnrealVTDbg 代码规范检查 ==="
echo "规则: 单个文件不超过300行（第三方库除外）"
echo ""

# 排除第三方库和自动生成文件
find . \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) \
  ! -path "*/ia32-doc/*" \
  ! -path "*/ntos/inc/*" \
  ! -path "*/Phnt/*" \
  ! -path "*/TinyXML/*" \
  ! -path "*/NetVerHelp/*" \
  ! -path "*/.git/*" \
  ! -path "*/.claude/*" \
  ! -path "*/Detours/*" \
  ! -path "*/json/*" \
  -exec wc -l {} + 2>/dev/null | \
  awk '$1 > 300 && $2 !~ /total/ {printf "%-6d %s\n", $1, $2}' | \
  sort -rn

echo ""
echo "=== 统计 ==="
total=$(find . \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) \
  ! -path "*/ia32-doc/*" \
  ! -path "*/ntos/inc/*" \
  ! -path "*/Phnt/*" \
  ! -path "*/TinyXML/*" \
  ! -path "*/NetVerHelp/*" \
  ! -path "*/.git/*" \
  ! -path "*/.claude/*" \
  ! -path "*/Detours/*" \
  ! -path "*/json/*" \
  2>/dev/null | wc -l)

violations=$(find . \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) \
  ! -path "*/ia32-doc/*" \
  ! -path "*/ntos/inc/*" \
  ! -path "*/Phnt/*" \
  ! -path "*/TinyXML/*" \
  ! -path "*/NetVerHelp/*" \
  ! -path "*/.git/*" \
  ! -path "*/.claude/*" \
  ! -path "*/Detours/*" \
  ! -path "*/json/*" \
  -exec wc -l {} + 2>/dev/null | \
  awk '$1 > 300 && $2 !~ /total/ {print}' | wc -l)

echo "总文件数: $total"
echo "违规文件: $violations"
echo "合规率: $(awk "BEGIN {printf \"%.1f%%\", (1 - $violations/$total) * 100}")"
