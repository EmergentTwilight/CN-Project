#!/bin/bash

# NS-3 安装路径
NS3_DIR="/home/zyb/ns-3.41"
SCRATCH_DIR="$NS3_DIR/scratch"
CODE_DIR="$(cd "$(dirname "$0")" && pwd)"

# 检查 NS-3 目录是否存在
if [ ! -d "$NS3_DIR" ]; then
    echo "错误: 找不到 NS-3 目录: $NS3_DIR"
    echo "请确认 NS-3 已正确安装，或修改脚本中的 NS3_DIR 变量"
    exit 1
fi

# 检查 code 目录是否存在
if [ ! -d "$CODE_DIR" ]; then
    echo "错误: 找不到 code 目录: $CODE_DIR"
    exit 1
fi

echo "=========================================="
echo "复制代码到 NS-3 scratch 目录..."
echo "=========================================="

# 备份 scratch 目录（可选）
if [ ! -d "$SCRATCH_DIR/backup" ]; then
    echo "创建 scratch 备份..."
    mkdir -p "$SCRATCH_DIR/backup"
    cp -r "$SCRATCH_DIR"/* "$SCRATCH_DIR/backup/" 2>/dev/null || true
fi

# 清理 scratch 目录中的旧文件（保留 backup 和 .gitignore）
echo "清理 scratch 目录中的旧文件..."
find "$SCRATCH_DIR" -mindepth 1 -maxdepth 1 ! -name "backup" ! -name ".gitignore" -exec rm -rf {} +

# 复制 code 目录内容到 scratch
echo "正在复制 $CODE_DIR 到 $SCRATCH_DIR ..."
cp -r "$CODE_DIR"/* "$SCRATCH_DIR/"
if [ $? -ne 0 ]; then
    echo "错误: 复制文件失败"
    exit 1
fi
echo "✓ 文件复制成功"

# 进入 NS-3 目录并构建
echo ""
echo "=========================================="
echo "编译程序..."
echo "=========================================="
cd "$NS3_DIR"
./ns3 build 2>&1 | grep -E "(main|algorithm-lib|error|Error|ERROR|Building|Linking)" | tail -20

# 检查构建是否成功
if [ $? -ne 0 ]; then
    echo ""
    echo "⚠ 构建过程中可能有错误，但继续尝试运行..."
fi

# 运行程序
echo ""
echo "=========================================="
echo "运行程序: scratch/main"
echo "=========================================="
./ns3 run scratch/main --no-build

echo ""
echo "=========================================="
echo "✓ 运行完成"
echo "=========================================="
echo ""
echo "提示: 可以使用以下参数控制程序行为:"
echo "  --correctness=true/false    - 运行正确性测试"
echo "  --performance=true/false    - 运行性能测试"
echo "  --report=true/false         - 生成性能报告"
echo ""
echo "示例: ./ns3 run 'scratch/main --correctness=true --performance=true'"

