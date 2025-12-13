#!/bin/bash

# NS-3 安装路径
NS3_DIR="/home/zyb/ns-3.41"
SCRATCH_DIR="$NS3_DIR/scratch"
SOURCE_FILE="dijkstra_example.cc"
TARGET_FILE="$SCRATCH_DIR/dijkstra-example.cc"

# 检查 NS-3 目录是否存在
if [ ! -d "$NS3_DIR" ]; then
    echo "错误: 找不到 NS-3 目录: $NS3_DIR"
    echo "请确认 NS-3 已正确安装，或修改脚本中的 NS3_DIR 变量"
    exit 1
fi

# 检查源文件是否存在
if [ ! -f "$SOURCE_FILE" ]; then
    echo "错误: 找不到源文件: $SOURCE_FILE"
    echo "请确保在 dijkstra 目录下运行此脚本"
    exit 1
fi

# 复制文件到 scratch 目录
echo "正在复制 $SOURCE_FILE 到 $SCRATCH_DIR ..."
cp "$SOURCE_FILE" "$TARGET_FILE"
if [ $? -ne 0 ]; then
    echo "错误: 复制文件失败"
    exit 1
fi
echo "✓ 文件复制成功"

# 进入 NS-3 目录并构建
echo "正在编译 dijkstra-example ..."
cd "$NS3_DIR"
./ns3 build 2>&1 | grep -E "(dijkstra|error|Error|ERROR)" || true

# 运行程序
echo ""
echo "正在运行 dijkstra-example ..."
echo "=========================================="
./ns3 run scratch/dijkstra-example --no-build
echo "=========================================="
echo "✓ 运行完成"

