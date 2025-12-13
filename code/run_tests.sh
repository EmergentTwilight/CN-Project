#!/bin/bash

# NS-3 安装路径
NS3_DIR="/home/zyb/ns-3.41"
SCRATCH_DIR="$NS3_DIR/scratch"
CODE_DIR="$(cd "$(dirname "$0")" && pwd)"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 显示帮助信息
show_help() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Breaking the Sorting Barrier 测试脚本${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "用法: $0 [选项] [测试模式]"
    echo ""
    echo "测试模式:"
    echo "  quick          - 快速测试（只运行正确性测试）"
    echo "  performance    - 性能测试（只运行性能对比）"
    echo "  report         - 生成报告（只生成性能报告）"
    echo "  full           - 完整测试（所有测试，默认）"
    echo "  custom         - 自定义测试（使用提供的参数）"
    echo ""
    echo "选项:"
    echo "  -h, --help             显示此帮助信息"
    echo "  -s, --size <数字>      设置测试规模（默认: 100）"
    echo "  -c, --correctness      运行正确性测试"
    echo "  -p, --performance      运行性能测试"
    echo "  -r, --report           生成性能报告"
    echo "  -n, --no-build         不重新编译（使用已编译的程序）"
    echo "  -v, --verbose          显示详细输出"
    echo ""
    echo "示例:"
    echo "  $0 quick                # 快速测试"
    echo "  $0 performance         # 性能测试"
    echo "  $0 full                # 完整测试"
    echo "  $0 custom --size=200   # 自定义测试，规模200"
    echo "  $0 -c -p --size=50     # 运行正确性和性能测试，规模50"
    echo ""
}

# 检查 NS-3 目录
check_ns3() {
    if [ ! -d "$NS3_DIR" ]; then
        echo -e "${RED}错误: 找不到 NS-3 目录: $NS3_DIR${NC}"
        echo "请确认 NS-3 已正确安装，或修改脚本中的 NS3_DIR 变量"
        exit 1
    fi
}

# 准备代码
prepare_code() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}准备代码...${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # 清理 scratch 目录中的旧文件（保留 backup 和 .gitignore）
    if [ "$SKIP_COPY" != "true" ]; then
        echo "清理 scratch 目录..."
        find "$SCRATCH_DIR" -mindepth 1 -maxdepth 1 ! -name "backup" ! -name ".gitignore" -exec rm -rf {} + 2>/dev/null
        
        # 复制代码
        echo "复制代码到 scratch 目录..."
        cp -r "$CODE_DIR"/* "$SCRATCH_DIR/" 2>/dev/null
        if [ $? -ne 0 ]; then
            echo -e "${RED}错误: 复制文件失败${NC}"
            exit 1
        fi
        echo -e "${GREEN}✓ 代码准备完成${NC}"
    fi
}

# 编译程序
build_program() {
    if [ "$NO_BUILD" != "true" ]; then
        echo ""
        echo -e "${BLUE}========================================${NC}"
        echo -e "${BLUE}编译程序...${NC}"
        echo -e "${BLUE}========================================${NC}"
        cd "$NS3_DIR"
        
        if [ "$VERBOSE" = "true" ]; then
            ./ns3 build
        else
            ./ns3 build 2>&1 | grep -E "(main|algorithm-lib|error|Error|ERROR|Building|Linking)" | tail -20
        fi
        
        if [ $? -ne 0 ]; then
            echo -e "${YELLOW}⚠ 构建过程中可能有警告，但继续尝试运行...${NC}"
        else
            echo -e "${GREEN}✓ 编译完成${NC}"
        fi
    fi
}

# 运行测试
run_test() {
    local test_args="$1"
    
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}运行测试...${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "参数: ${YELLOW}$test_args${NC}"
    echo ""
    
    cd "$NS3_DIR"
    
    if [ "$VERBOSE" = "true" ]; then
        ./ns3 run "scratch/main $test_args" --no-build
    else
        ./ns3 run "scratch/main $test_args" --no-build 2>&1 | \
            grep -v "^Waf:" | \
            grep -v "^'configure'" | \
            grep -v "^'build' finished"
    fi
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ 测试完成${NC}"
    echo -e "${GREEN}========================================${NC}"
}

# 解析命令行参数
TEST_MODE="full"
CUSTOM_ARGS=""
NO_BUILD="false"
VERBOSE="false"
SKIP_COPY="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        quick)
            TEST_MODE="quick"
            shift
            ;;
        performance)
            TEST_MODE="performance"
            shift
            ;;
        report)
            TEST_MODE="report"
            shift
            ;;
        full)
            TEST_MODE="full"
            shift
            ;;
        custom)
            TEST_MODE="custom"
            shift
            ;;
        -s|--size)
            CUSTOM_ARGS="$CUSTOM_ARGS --size=$2"
            shift 2
            ;;
        -c|--correctness)
            CUSTOM_ARGS="$CUSTOM_ARGS --correctness=true"
            shift
            ;;
        -p|--performance)
            CUSTOM_ARGS="$CUSTOM_ARGS --performance=true"
            shift
            ;;
        -r|--report)
            CUSTOM_ARGS="$CUSTOM_ARGS --report=true"
            shift
            ;;
        --size=*)
            CUSTOM_ARGS="$CUSTOM_ARGS $1"
            shift
            ;;
        --correctness=*)
            CUSTOM_ARGS="$CUSTOM_ARGS $1"
            shift
            ;;
        --performance=*)
            CUSTOM_ARGS="$CUSTOM_ARGS $1"
            shift
            ;;
        --report=*)
            CUSTOM_ARGS="$CUSTOM_ARGS $1"
            shift
            ;;
        -n|--no-build)
            NO_BUILD="true"
            shift
            ;;
        -v|--verbose)
            VERBOSE="true"
            shift
            ;;
        --skip-copy)
            SKIP_COPY="true"
            shift
            ;;
        *)
            # 其他参数作为自定义参数
            CUSTOM_ARGS="$CUSTOM_ARGS $1"
            shift
            ;;
    esac
done

# 根据测试模式设置参数
case $TEST_MODE in
    quick)
        TEST_ARGS="--correctness=true --performance=false --report=false"
        ;;
    performance)
        TEST_ARGS="--correctness=false --performance=true --report=false"
        ;;
    report)
        TEST_ARGS="--correctness=false --performance=false --report=true"
        ;;
    full)
        TEST_ARGS="--correctness=true --performance=true --report=true"
        ;;
    custom)
        TEST_ARGS="$CUSTOM_ARGS"
        # 如果没有提供任何参数，使用默认值
        if [ -z "$CUSTOM_ARGS" ]; then
            TEST_ARGS="--correctness=true --performance=true --report=true"
        fi
        ;;
esac

# 合并自定义参数
if [ -n "$CUSTOM_ARGS" ] && [ "$TEST_MODE" != "custom" ]; then
    TEST_ARGS="$TEST_ARGS $CUSTOM_ARGS"
fi

# 执行测试
check_ns3
prepare_code
build_program
run_test "$TEST_ARGS"

# 显示生成的文件
echo ""
echo -e "${BLUE}生成的结果文件:${NC}"
cd "$NS3_DIR"
ls -lh performance_results*.{csv,json,txt} experiment_report.txt 2>/dev/null | \
    awk '{printf "  %s  %s\n", $5, $9}' || echo "  (无结果文件)"

echo ""
echo -e "${GREEN}完成！${NC}"

