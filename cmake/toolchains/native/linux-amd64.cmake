
# ============================================
# 本机构建工具链文件
# Linux AMD64（x86-64）
# ============================================
#
# 构建依赖：
#   - build-essential  (提供 gcc, g++, make 等基础工具)
#   - gcc              (C 编译器)
#   - g++              (C++ 编译器)
#   - ninja-build      (推荐使用 Ninja 构建系统以加速编译)
#
# 安装命令 (Ubuntu/Debian)：
#   sudo apt-get install build-essential gcc g++ ninja-build
#
# 使用此工具链：
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/native/linux-amd64.cmake ..
#   cmake --build . --parallel
#
# 使用 Ninja（推荐）：
#   cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/native/linux-amd64.cmake ..
#   ninja
# ============================================

# 目标系统（本地构建）
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 平台标识
set(CANGJIE_TARGET_OS "linux")
set(CANGJIE_TARGET_ARCH "amd64")
set(CANGJIE_OUTPUT_SUFFIX "linux_amd64")

# 对于本地 Linux 构建，使用默认的 GCC/Clang 编译器
# 可通过 CMAKE_C_COMPILER / CMAKE_CXX_COMPILER 覆盖

# AMD64 特定编译器参数
set(CMAKE_C_FLAGS_INIT "-m64 -march=x86-64")
set(CMAKE_CXX_FLAGS_INIT "-m64 -march=x86-64")

# 为共享库启用位置无关代码
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 不同构建类型的优化参数
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")

set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")

set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")

# Linux 特定宏定义
add_definitions(-D_GNU_SOURCE)
add_definitions(-DLINUX)

message(STATUS "====================================")
message(STATUS "本地 Linux AMD64 工具链：")
message(STATUS "  系统: Linux")
message(STATUS "  架构: x86_64 (AMD64)")
message(STATUS "  输出后缀: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")
