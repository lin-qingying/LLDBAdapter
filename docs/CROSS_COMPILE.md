# 交叉编译指南 (Cross-Compilation Guide)

本文档说明如何在不同平台上编译仓颉调试器，包括本地编译和交叉编译。

This document explains how to compile the Cangjie Debugger on different platforms, including native and cross-compilation.

## 目录 (Table of Contents)

1. [Windows AMD64 本地编译](#windows-amd64-本地编译)
2. [Linux ARM 交叉编译](#linux-arm-交叉编译)
3. [验证与部署](#验证编译结果)

---

## Windows AMD64 本地编译

### 使用 MinGW-w64 编译

#### 前置要求

安装 MinGW-w64 工具链（选择以下方式之一）：

**方式 1: 使用 MSYS2（推荐）**
```bash
# 下载并安装 MSYS2: https://www.msys2.org/

# 在 MSYS2 终端中安装 MinGW-w64
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

**方式 2: 独立 MinGW-w64 安装**
- 从 [WinLibs](https://winlibs.com/) 或 [MinGW-w64](https://www.mingw-w64.org/) 下载
- 将 bin 目录添加到 PATH 环境变量

**方式 3: 使用 CLion 自带的 MinGW**
- CLion 自带 MinGW 工具链，位于 `C:\Users\<用户名>\AppData\Local\Programs\CLion\bin\mingw`

#### 编译步骤

```bash
# 方法 1: 使用工具链文件
mkdir build-mingw
cd build-mingw
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-windows-amd64-mingw.cmake ..
cmake --build . --config Release

# 方法 2: 直接指定编译器（如果 MinGW 在 PATH 中）
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
cmake --build . --config Release

# 方法 3: 指定完整路径
cmake -G "Ninja" \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-windows-amd64-mingw.cmake \
      -DCMAKE_C_COMPILER="C:/msys64/mingw64/bin/gcc.exe" \
      -DCMAKE_CXX_COMPILER="C:/msys64/mingw64/bin/g++.exe" \
      ..
```

### 使用 MSVC 编译

#### 前置要求

安装 Visual Studio（2019 或更高版本）并包含以下组件：
- C++ 桌面开发工作负载
- Windows SDK
- CMake 工具（可选，也可以使用独立的 CMake）

#### 编译步骤

```bash
# 方法 1: 使用工具链文件
mkdir build-msvc
cd build-msvc
cmake -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-windows-amd64-msvc.cmake ..
cmake --build . --config Release

# 方法 2: 使用 Ninja（需要先运行 vcvarsall.bat）
# 在 Visual Studio Developer Command Prompt 中执行：
mkdir build-msvc-ninja
cd build-msvc-ninja
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-windows-amd64-msvc.cmake ..
cmake --build . --config Release

# 方法 3: 直接使用 MSBuild
cmake -G "Visual Studio 17 2022" -A x64 ..
msbuild CangJieLLDBAdapter.sln /p:Configuration=Release /m
```

### MinGW vs MSVC 对比

| 特性 | MinGW-w64 | MSVC |
|------|-----------|------|
| 许可证 | 开源 (GPL) | 商业软件 |
| 二进制兼容性 | GCC ABI | MSVC ABI |
| 静态链接 | ✅ 支持 | ✅ 支持 |
| 可执行文件大小 | 较小 | 较大 |
| 调试工具 | GDB | Visual Studio Debugger |
| 编译速度 | 较快 | 中等 |
| Windows API 支持 | ✅ 完全支持 | ✅ 原生支持 |
| 推荐场景 | 开源项目、跨平台 | Windows 专用、企业级 |

---

## Linux ARM 交叉编译

## 支持的目标架构 (Supported Target Architectures)

- **ARM64 (aarch64)**: 64位 ARM 架构（树莓派 4/5、华为鲲鹏等）
- **ARM32 (armhf)**: 32位 ARM 硬浮点架构（树莓派 2/3、嵌入式设备等）

## 前置要求 (Prerequisites)

### 安装交叉编译工具链 (Install Cross-Compilation Toolchains)

#### Ubuntu/Debian 系统:

```bash
# ARM64 工具链
sudo apt-get update
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# ARM32 工具链
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# 验证安装
aarch64-linux-gnu-gcc --version
arm-linux-gnueabihf-gcc --version
```

#### Fedora/RHEL 系统:

```bash
# ARM64 工具链
sudo dnf install gcc-aarch64-linux-gnu gcc-c++-aarch64-linux-gnu

# ARM32 工具链
sudo dnf install gcc-arm-linux-gnu gcc-c++-arm-linux-gnu
```

#### Arch Linux:

```bash
# 从 AUR 安装
yay -S aarch64-linux-gnu-gcc
yay -S arm-linux-gnueabihf-gcc
```

## ARM64 交叉编译 (ARM64 Cross-Compilation)

### 基本编译 (Basic Build)

```bash
# 创建构建目录
mkdir build-arm64
cd build-arm64

# 配置 CMake
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake ..

# 编译
cmake --build . -j$(nproc)

# 可执行文件位于
ls -lh ../output/CangJieLLDBAdapter
```

### 使用自定义编译器路径 (Custom Compiler Paths)

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ \
      ..
```

### 使用 Sysroot

如果您有 ARM64 目标系统的 sysroot（包含库和头文件）：

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_SYSROOT=/path/to/arm64-sysroot \
      -DCMAKE_FIND_ROOT_PATH=/path/to/arm64-sysroot \
      ..
```

### 指定优化级别

```bash
# Release 模式（默认）
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# Debug 模式
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
```

## ARM32 交叉编译 (ARM32 Cross-Compilation)

### 基本编译 (Basic Build)

```bash
# 创建构建目录
mkdir build-arm32
cd build-arm32

# 配置 CMake
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm32.cmake ..

# 编译
cmake --build . -j$(nproc)

# 可执行文件位于
ls -lh ../output/CangJieLLDBAdapter
```

### 针对特定 ARM 处理器优化

编辑 `cmake/toolchain-linux-arm32.cmake` 中的编译器标志：

```cmake
# 针对 Cortex-A9
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mtune=cortex-a9 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mtune=cortex-a9 -mfpu=neon-vfpv4 -mfloat-abi=hard")

# 针对 Cortex-A7 (树莓派 2)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
```

## 工具链文件说明 (Toolchain File Reference)

### toolchain-linux-arm64.cmake

该文件配置从 Linux AMD64 到 ARM64 的交叉编译环境：

**关键配置：**
- 目标系统：Linux
- 目标处理器：aarch64
- 默认编译器：aarch64-linux-gnu-gcc/g++
- 默认架构标志：-march=armv8-a

### toolchain-linux-arm32.cmake

该文件配置从 Linux AMD64 到 ARM32 的交叉编译环境：

**关键配置：**
- 目标系统：Linux
- 目标处理器：arm
- 默认编译器：arm-linux-gnueabihf-gcc/g++
- 默认架构标志：-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard

## 验证编译结果 (Verify Build)

### 检查目标架构

```bash
# 检查可执行文件架构
file output/CangJieLLDBAdapter

# ARM64 输出示例：
# output/CangJieLLDBAdapter: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux)

# ARM32 输出示例：
# output/CangJieLLDBAdapter: ELF 32-bit LSB executable, ARM, EABI5 version 1 (GNU/Linux)
```

### 检查依赖库

```bash
# 使用 ARM 版本的 ldd（如果有 sysroot）
aarch64-linux-gnu-readelf -d output/CangJieLLDBAdapter

# 或使用 objdump
aarch64-linux-gnu-objdump -p output/CangJieLLDBAdapter | grep NEEDED
```

## 部署到目标设备 (Deploy to Target Device)

### 复制可执行文件

```bash
# 使用 scp
scp output/CangJieLLDBAdapter user@arm-device:/path/to/destination/

# 或使用 rsync
rsync -avz output/CangJieLLDBAdapter user@arm-device:/path/to/destination/
```

### 复制依赖库

如果目标设备缺少某些库：

```bash
# 查找需要的共享库
aarch64-linux-gnu-readelf -d output/CangJieLLDBAdapter | grep NEEDED

# 从交叉编译环境复制必要的库
scp /usr/aarch64-linux-gnu/lib/libstdc++.so.6 user@arm-device:/usr/lib/
scp /usr/aarch64-linux-gnu/lib/libgcc_s.so.1 user@arm-device:/usr/lib/
```

### 在目标设备上运行

```bash
# SSH 到目标设备
ssh user@arm-device

# 设置执行权限
chmod +x /path/to/CangJieLLDBAdapter

# 运行调试器（需要端口号）
/path/to/CangJieLLDBAdapter 8080
```

## 常见问题 (Troubleshooting)

### 找不到交叉编译器

**错误**: `Could not find aarch64-linux-gnu-gcc`

**解决方案**:
```bash
# 检查工具链是否安装
which aarch64-linux-gnu-gcc

# 如果未安装，使用包管理器安装
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### Protobuf 编译失败

**问题**: Protobuf 在交叉编译时可能遇到问题

**解决方案**:
1. 首先在宿主机上构建 protoc：
```bash
cd third_party/protobuf
mkdir build-host && cd build-host
cmake .. -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF
cmake --build . --target protoc
```

2. 然后在交叉编译时使用宿主机的 protoc：
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DPROTOBUF_PROTOC_EXECUTABLE=/path/to/host/protoc \
      ..
```

### 链接错误

**问题**: 交叉编译时找不到某些库

**解决方案**:
- 确保在 sysroot 中有目标架构的库
- 或静态链接相关库（修改 CMakeLists.txt）

### LLDB 库不兼容

**问题**: 目标设备上 LLDB 版本不匹配

**解决方案**:
- 确保交叉编译时使用的 LLDB 头文件与目标设备上的 liblldb 版本匹配
- 或将 ARM 版本的 liblldb 打包到发布包中

## 性能优化建议 (Performance Optimization)

### ARM64 优化

```bash
# 针对特定 CPU 核心优化（如华为鲲鹏 920）
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_C_FLAGS="-march=armv8.2-a+crypto+fp16 -mtune=tsv110" \
      -DCMAKE_CXX_FLAGS="-march=armv8.2-a+crypto+fp16 -mtune=tsv110" \
      ..
```

### ARM32 优化

```bash
# 针对树莓派 3 (Cortex-A53) 优化
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm32.cmake \
      -DCMAKE_C_FLAGS="-march=armv8-a -mtune=cortex-a53 -mfpu=crypto-neon-fp-armv8" \
      -DCMAKE_CXX_FLAGS="-march=armv8-a -mtune=cortex-a53 -mfpu=crypto-neon-fp-armv8" \
      ..
```

## 参考资料 (References)

- [CMake Toolchain Files Documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [ARM GCC Options](https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html)
- [Debian ARM Ports](https://wiki.debian.org/ArmPorts)
- [Cross-Compilation Best Practices](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html)

## 技术支持 (Support)

如有问题，请访问项目仓库提交 Issue 或查看文档：
- 主 README: [README.md](../README.md)
- 中文 README: [README_zh.md](../README_zh.md)
- 构建指南: [CLAUDE.md](../CLAUDE.md)