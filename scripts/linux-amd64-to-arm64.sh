#!/bin/bash
# ============================================
# Linux AMD64 → Linux ARM64 交叉编译脚本
# ============================================
# 此脚本实现两阶段编译：
# 1. 在宿主机（Linux AMD64）使用系统编译器编译 protobuf 和 protoc
# 2. 使用宿主机 protoc 编译 proto 文件
# 3. 使用 ARM64 交叉编译器编译目标项目
# ============================================

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 配置变量
BUILD_TYPE="${BUILD_TYPE:-Release}"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/toolchains/linux-arm64.cmake"
HOST_BUILD_DIR="${PROJECT_ROOT}/cmake-build-linux-amd64"
TARGET_BUILD_DIR="${PROJECT_ROOT}/cmake-build-linux-arm64"

# 宿主机编译器配置（使用系统编译器）
HOST_CC="${HOST_CC:-/usr/bin/gcc}"
HOST_CXX="${HOST_CXX:-/usr/bin/g++}"
HOST_GENERATOR="${HOST_GENERATOR:-Ninja}"

# 打印标题
echo ""
echo "========================================"
echo "  Linux AMD64 → ARM64 交叉编译工具"
echo "========================================"
echo ""

# 打印配置信息
print_info "配置信息："
print_info "  项目根目录: ${PROJECT_ROOT}"
print_info "  构建类型: ${BUILD_TYPE}"
print_info "  宿主机构建目录: ${HOST_BUILD_DIR}"
print_info "  目标构建目录: ${TARGET_BUILD_DIR}"
print_info "  工具链文件: ${TOOLCHAIN_FILE}"
print_info "  宿主机 C 编译器: ${HOST_CC}"
print_info "  宿主机 C++ 编译器: ${HOST_CXX}"
print_info "  宿主机生成器: ${HOST_GENERATOR}"
echo ""

# 检查必要的工具
print_step "检查编译环境..."

if ! command -v cmake &> /dev/null; then
    print_error "未找到 cmake，请先安装 cmake"
    print_info "安装命令: sudo apt-get install cmake"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n 1 | awk '{print $3}')
print_info "✓ CMake 版本: ${CMAKE_VERSION}"

# 检查宿主机编译器
if [ ! -x "${HOST_CC}" ]; then
    print_error "未找到宿主机 C 编译器: ${HOST_CC}"
    print_info "安装命令: sudo apt-get install gcc"
    exit 1
fi

if [ ! -x "${HOST_CXX}" ]; then
    print_error "未找到宿主机 C++ 编译器: ${HOST_CXX}"
    print_info "安装命令: sudo apt-get install g++"
    exit 1
fi

GCC_VERSION=$(${HOST_CC} --version | head -n 1 | awk '{print $3}')
print_info "✓ 宿主机 GCC 版本: ${GCC_VERSION}"

# 检查 Ninja（如果指定使用）
if [ "${HOST_GENERATOR}" = "Ninja" ]; then
    if ! command -v ninja &> /dev/null; then
        print_warn "未找到 ninja，将使用 Unix Makefiles"
        HOST_GENERATOR="Unix Makefiles"
    else
        NINJA_VERSION=$(ninja --version)
        print_info "✓ Ninja 版本: ${NINJA_VERSION}"
    fi
fi

# 检查 ARM64 交叉编译器
if ! command -v aarch64-linux-gnu-gcc &> /dev/null; then
    print_error "未找到 aarch64-linux-gnu-gcc 交叉编译器"
    print_error "请运行: sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
    exit 1
fi

ARM_GCC_VERSION=$(aarch64-linux-gnu-gcc --version | head -n 1 | awk '{print $3}')
print_info "✓ ARM64 GCC 版本: ${ARM_GCC_VERSION}"

print_info "✓ 所有必要工具已就绪"
echo ""

# ============================================
# 阶段 1: 在宿主机编译 protobuf 和 protoc
# ============================================
print_step "======================================"
print_step "阶段 1/3: 编译宿主机 Protobuf 和 protoc"
print_step "======================================"
echo ""

mkdir -p "${HOST_BUILD_DIR}"
cd "${HOST_BUILD_DIR}"

# 检查是否已经构建过
PROTOC_HOST="${HOST_BUILD_DIR}/third_party/protobuf-install/bin/protoc"
if [ -f "${PROTOC_HOST}" ]; then
    print_info "检测到已存在的宿主机 protoc: ${PROTOC_HOST}"
    print_warn "跳过宿主机 protobuf 构建（如需重新构建，请删除 ${HOST_BUILD_DIR}）"
else
    print_info "配置宿主机构建环境..."
    print_info "  使用生成器: ${HOST_GENERATOR}"
    print_info "  使用编译器: ${HOST_CC} / ${HOST_CXX}"
    print_info "  注意：Protobuf 会在配置阶段自动编译"

    cmake -G "${HOST_GENERATOR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="${HOST_CC}" \
        -DCMAKE_CXX_COMPILER="${HOST_CXX}" \
        "${PROJECT_ROOT}"

    # BuildProtobuf.cmake 已经在配置阶段编译了 protobuf
    # 这里只是验证 protoc 是否成功生成

    if [ ! -f "${PROTOC_HOST}" ]; then
        print_error "宿主机 protoc 编译失败: ${PROTOC_HOST}"
        exit 1
    fi

    print_info "✓ 宿主机 protoc 编译成功"
fi

PROTOC_VERSION=$("${PROTOC_HOST}" --version | awk '{print $2}')
print_info "✓ protoc 版本: ${PROTOC_VERSION}"
print_info "✓ protoc 路径: ${PROTOC_HOST}"
echo ""

# ============================================
# 阶段 2: 使用宿主机 protoc 生成 proto 文件
# ============================================
print_step "======================================"
print_step "阶段 2/3: 生成 Protobuf C++ 代码"
print_step "======================================"
echo ""

PROTO_OUTPUT_DIR="${TARGET_BUILD_DIR}/generated/proto"
mkdir -p "${PROTO_OUTPUT_DIR}"

print_info "Proto 文件信息："
print_info "  输入目录: ${PROJECT_ROOT}/schema"
print_info "  输出目录: ${PROTO_OUTPUT_DIR}"

cd "${PROJECT_ROOT}/schema"

# 列出要编译的 proto 文件
PROTO_FILES=(event.proto model.proto request.proto response.proto)
print_info "  待编译文件: ${PROTO_FILES[*]}"
echo ""

print_info "开始生成 C++ 代码..."
"${PROTOC_HOST}" \
    --cpp_out="${PROTO_OUTPUT_DIR}" \
    --proto_path="${PROJECT_ROOT}/schema" \
    "${PROTO_FILES[@]}"

if [ $? -eq 0 ]; then
    GENERATED_COUNT=$(ls -1 "${PROTO_OUTPUT_DIR}"/*.pb.cc 2>/dev/null | wc -l)
    print_info "✓ Protobuf 文件生成成功（生成 ${GENERATED_COUNT} 个 .pb.cc 文件）"
else
    print_error "Protobuf 文件生成失败"
    exit 1
fi
echo ""

# ============================================
# 阶段 3: 交叉编译目标项目到 ARM64
# ============================================
print_step "======================================"
print_step "阶段 3/3: 交叉编译项目到 ARM64"
print_step "======================================"
echo ""

mkdir -p "${TARGET_BUILD_DIR}"
cd "${TARGET_BUILD_DIR}"

print_info "配置交叉编译环境..."
print_info "  宿主系统: Linux AMD64"
print_info "  目标系统: Linux ARM64 (aarch64)"
print_info "  交叉编译器: aarch64-linux-gnu-gcc"
print_info "  宿主机 protoc: ${PROTOC_HOST}"
echo ""

cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DPROTOC_HOST="${PROTOC_HOST}" \
    "${PROJECT_ROOT}"

print_info "开始交叉编译（并行编译，使用 $(nproc) 个核心）..."
cmake --build . --config "${BUILD_TYPE}" --parallel $(nproc)

if [ $? -eq 0 ]; then
    echo ""
    print_step "======================================"
    print_step "✓ 交叉编译成功完成！"
    print_step "======================================"
    echo ""
    print_info "输出信息："
    print_info "  输出目录: ${PROJECT_ROOT}/output"

    # 列出生成的文件
    if [ -d "${PROJECT_ROOT}/output" ]; then
        print_info "  生成的可执行文件："
        ls -lh "${PROJECT_ROOT}/output" | grep -E "CangJie.*arm64" | awk '{printf "    %s  %s\n", $5, $9}'

        # 验证 ARM64 架构
        ARM64_BIN=$(ls "${PROJECT_ROOT}/output"/CangJieLLDBAdapter_*arm64* 2>/dev/null | head -n 1)
        if [ -n "${ARM64_BIN}" ]; then
            FILE_INFO=$(file "${ARM64_BIN}")
            if echo "${FILE_INFO}" | grep -q "ARM aarch64"; then
                print_info "  ✓ 架构验证: ARM64 (aarch64)"
            else
                print_warn "  ⚠ 架构验证失败，请检查: ${FILE_INFO}"
            fi
        fi
    fi
else
    print_error "交叉编译失败"
    exit 1
fi

echo ""
print_step "======================================"
print_step "全部完成！"
print_step "======================================"
echo ""

# 打印使用提示
print_info "使用方法："
print_info "  1. 将可执行文件复制到 ARM64 Linux 设备"
print_info "  2. 确保 ARM64 设备上有 LLDB 库（liblldb_linux_arm64.so）"
print_info "  3. 运行: ./CangJieLLDBAdapter_linux_arm64 <端口号>"
echo ""

print_info "清理构建目录："
print_info "  宿主机构建: rm -rf ${HOST_BUILD_DIR}"
print_info "  目标构建: rm -rf ${TARGET_BUILD_DIR}"
echo ""