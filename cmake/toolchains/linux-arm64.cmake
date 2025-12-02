
# ============================================
# 交叉编译工具链文件
# Linux AMD64 -> Linux ARM64 (aarch64)
# ============================================
#
# 构建依赖：
#   - gcc-aarch64-linux-gnu   (ARM64 交叉编译 C 编译器)
#   - g++-aarch64-linux-gnu   (ARM64 交叉编译 C++ 编译器)
#   - ninja-build             (推荐使用 Ninja 构建系统以加速编译)
#
# 安装命令 (Ubuntu/Debian)：
#   sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu ninja-build
#
# 使用此工具链：
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake ..
#   cmake --build . --parallel
#
# 使用 Ninja（推荐）：
#   cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake ..
#   ninja
#
# 自定义编译器路径：
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake \
#         -DCMAKE_C_COMPILER=/custom/path/aarch64-linux-gnu-gcc \
#         -DCMAKE_CXX_COMPILER=/custom/path/aarch64-linux-gnu-g++ \
#         ..
#
# 使用 sysroot（可选）：
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake \
#         -DCMAKE_SYSROOT=/path/to/arm64/sysroot \
#         ..
# ============================================

# 目标系统（交叉编译到 ARM64 Linux）
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 平台标识
set(CANGJIE_TARGET_OS "linux")
set(CANGJIE_TARGET_ARCH "arm64")
set(CANGJIE_OUTPUT_SUFFIX "linux_arm64")

# 交叉编译器设置
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 交叉编译模式（重要：防止 CMake 尝试在目标平台运行测试程序）
set(CMAKE_CROSSCOMPILING TRUE)

# 查找程序的目标路径（仅在宿主系统查找）
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# 查找库和头文件的目标路径（仅在目标系统查找）
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ARM64 特定编译器参数
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")

# 为共享库启用位置无关代码
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 不同构建类型的优化参数
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=armv8-a")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=armv8-a")

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
message(STATUS "交叉编译 Linux ARM64 工具链：")
message(STATUS "  宿主系统: ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "  目标系统: Linux")
message(STATUS "  目标架构: aarch64 (ARM64)")
message(STATUS "  C 编译器: ${CMAKE_C_COMPILER}")
message(STATUS "  C++ 编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "  输出后缀: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")