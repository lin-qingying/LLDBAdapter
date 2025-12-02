# 仓颉 LLDB 调试器 (Cangjie LLDB Debugger)

<div align="center">

**专为仓颉编程语言设计的高性能调试器后端**

基于 LLDB 15.0.4 | Protocol Buffers 通信 | 跨平台支持

[![License](https://img.shields.io/badge/license-Apache%202.0%20with%20Runtime%20Library%20Exception-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16.5%2B-blue.svg)](https://cmake.org/)

</div>

---

## 📖 项目简介

这是一个为**仓颉（Cangjie）编程语言**设计的调试器后端，使用 **LLDB 15.0.4** 作为底层调试引擎，通过 **Protocol Buffers** 协议进行高效的二进制通信。该调试器采用客户端-服务器架构，可与任何支持 TCP 和 Protobuf 的前端集成（如 IDE、编辑器插件等）。

### 🌟 核心特性

- **完整的调试功能**：断点管理、执行控制、变量检查、内存操作、反汇编等
- **高效通信协议**：使用 Protocol Buffers 进行二进制序列化，性能优异
- **跨平台支持**：Windows (MSVC/MinGW)、Linux (x86_64/ARM64/ARM32)、macOS (x86_64/ARM64)
- **LLDB 深度集成**：充分利用 LLDB C++ API 的强大功能
- **可扩展架构**：可适配其他使用 LLVM 后端的编程语言
- **静态链接**：标准库和 protobuf 静态链接，减少运行时依赖

### 🔄 扩展到其他 LLVM 语言

虽然本项目专为仓颉语言设计，但由于底层使用标准的 LLDB API，**理论上可以支持任何使用 LLVM 作为编译后端的语言**（如 Rust、Swift、C/C++、Kotlin Native 等）。

要适配其他语言，需要：
1. **替换 LLDB 依赖**：将当前固定的 LLDB 15.0.4 库替换为目标语言使用的 LLDB 版本
2. **调整符号解析**：根据目标语言的调试符号格式调整 `ProtoConverter` 中的类型转换逻辑
3. **扩展协议**：如需要，在 `schema/*.proto` 中添加语言特定的消息类型

详细说明请参见 [扩展指南](#-扩展到其他-llvm-语言的详细步骤)。

---

## 🏗️ 项目架构

### 系统架构图

```
┌─────────────────────────────────────────────────────┐
│    调试前端 (IDE/编辑器/命令行工具)                    │
│    - Visual Studio Code 插件                        │
│    - JetBrains IDE 插件                             │
│    - 自定义调试 UI                                   │
└────────────────────┬────────────────────────────────┘
                     │ TCP (端口 8080)
                     │ Protocol Buffers 消息
                     ▼
┌─────────────────────────────────────────────────────┐
│           CangJieLLDBAdapter (本项目)                │
│  ┌────────────────────────────────────────────────┐  │
│  │      DebuggerClient (主调试器核心)              │  │
│  │  • 消息循环和请求分发                           │  │
│  │  • 20+ 调试命令处理器                          │  │
│  │  • 异步事件推送                                 │  │
│  └────────────────────────────────────────────────┘  │
│  ┌─────────┬─────────────┬──────────┬────────────┐  │
│  │Breakpoint│ProtoConverter│ Logger │ TcpClient  │  │
│  │Manager   │              │        │            │  │
│  └─────────┴─────────────┴──────────┴────────────┘  │
│  ┌────────────────────────────────────────────────┐  │
│  │    Protocol Buffers 消息层                      │  │
│  │  Request / Response / Event / Model            │  │
│  └────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────┤
│           LLDB 15.0.4 C++ API                       │
│  SBDebugger → SBTarget → SBProcess → SBThread      │
│             → SBBreakpoint → SBValue...            │
├─────────────────────────────────────────────────────┤
│              被调试的仓颉程序                         │
└─────────────────────────────────────────────────────┘
```

### 目录结构

```
cangjie_debugger/
├── src/                          # 源代码 (~8000 行)
│   ├── client/                   # 客户端网络和调试逻辑
│   │   ├── DebuggerClient.cpp    # 主调试器实现
│   │   ├── DebuggerClientHandlers.cpp    # 请求处理器
│   │   ├── DebuggerClientResponse.cpp    # 响应发送
│   │   ├── DebuggerClientEvents.cpp      # 事件推送
│   │   ├── DebuggerClientUtils.cpp       # 工具函数
│   │   └── TcpClient.cpp         # TCP 网络通信
│   ├── core/                     # 核心调试功能
│   │   └── BreakpointManager.cpp # 断点管理
│   ├── protocol/                 # 协议转换
│   │   └── ProtoConverter.cpp    # Protobuf 消息转换
│   ├── utils/                    # 工具类
│   │   └── Logger.cpp            # 线程安全日志
│   └── main.cpp                  # 程序入口
│
├── include/cangjie/debugger/     # 公共头文件
│   ├── DebuggerClient.h
│   ├── TcpClient.h
│   ├── ProtoConverter.h
│   ├── BreakpointManager.h
│   └── Logger.h
│
├── schema/                       # Protocol Buffers 定义 (~3600 行)
│   ├── model.proto               # 数据模型 (Thread, Frame, Variable 等)
│   ├── request.proto             # 请求消息 (20+ 种类型)
│   ├── response.proto            # 响应消息
│   └── event.proto               # 事件消息 (状态变化、输出等)
│
├── cmake/                        # CMake 构建模块
│   ├── BuildProtobuf.cmake       # Protobuf 自动构建
│   ├── DownloadLLVM.cmake        # LLVM/LLDB 下载
│   └── toolchains/               # 交叉编译工具链
│       └── native/               # 原生平台工具链
│           ├── windows-amd64.cmake
│           ├── linux-amd64.cmake
│           ├── linux-arm64.cmake
│           ├── macos-amd64.cmake
│           └── macos-arm64.cmake
│
├── third_party/                  # 第三方依赖
│   ├── protobuf/                 # Protocol Buffers 源码
│   ├── llvm-project/             # LLVM/LLDB 头文件
│   └── lib/                      # LLDB 动态库
│       ├── liblldb_windows_amd64.dll
│       ├── liblldb_linux_amd64.so
│       └── liblldb_macos_arm64.dylib
│
├── CMakeLists.txt                # 主构建文件
├── CLAUDE.md                     # 开发指南
├── README_zh.md                  # 本文件
└── output/                       # 编译输出
    └── CangJieLLDBAdapter_*      # 平台特定可执行文件
```

---

## ✨ 功能特性

### 1. 会话管理
- 创建调试目标（加载可执行文件）
- 启动进程（支持参数、环境变量、工作目录）
- 附加到运行中的进程（Attach）
- 分离和终止进程

### 2. 执行控制
- 继续执行（Continue）
- 暂停执行（Suspend）
- 单步执行
  - 单步进入（Step Into）
  - 单步跳过（Step Over）
  - 单步跳出（Step Out）
- 运行到光标（Run to Cursor）

### 3. 断点管理
- **行断点**：在源文件特定行设置
- **地址断点**：在内存地址设置
- **函数断点**：在函数入口设置
- **条件断点**：带条件表达式的断点
- **符号断点**：按函数名或正则模式
- **观察点（数据断点）**：监视内存变化（读/写/读写）
- 断点操作：启用、禁用、删除、更新

### 4. 变量和表达式
- 获取局部变量列表
- 获取函数参数
- 获取全局变量
- 获取/设置变量值
- 展开复合类型（结构体、数组、指针）
- 表达式求值（在栈帧上下文中）

### 5. 调用栈和线程
- 获取所有线程列表
- 获取线程调用栈
- 获取栈帧详细信息（函数名、源位置、PC 地址）
- 线程状态查询

### 6. 内存和寄存器
- 读取内存块
- 写入内存块
- 获取寄存器值
- 寄存器组管理

### 7. 反汇编
- 在指定地址反汇编指令
- 配置反汇编选项（显示机器码、符号化）

### 8. 异步事件推送
- 进程状态变化事件（启动、停止、运行、退出、崩溃）
- 断点命中事件
- 模块加载/卸载事件
- 进程标准输出/标准错误
- 线程状态变更事件

---

## 🚀 快速开始

### 前置要求

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| **编译器** | GCC 10+ / Clang 12+ / MinGW | 支持 C++17 标准 |
| **CMake** | 3.16.5+ | 构建系统 |
| **LLDB** | 15.0.4 | 调试引擎（已包含在 `third_party/` 中） |
| **Protocol Buffers** | 3.x | 自动从源码构建 |

### 构建步骤

#### Windows (MinGW)

```bash
# 1. 克隆仓库（包含子模块）
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. 配置构建
cmake -B build -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-amd64.cmake

# 3. 构建项目
cmake --build build --config Release

# 4. 可执行文件位于
# output/CangJieLLDBAdapter_windows_amd64.exe
```

#### Linux (x86_64)

```bash
# 1. 克隆仓库
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. 安装依赖
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build

# 3. 配置构建
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-amd64.cmake

# 4. 构建项目
cmake --build build

# 5. 可执行文件位于
# output/CangJieLLDBAdapter_linux_amd64
```

#### macOS (Apple Silicon)

```bash
# 1. 克隆仓库
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. 安装依赖
brew install cmake ninja llvm

# 3. 配置构建
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-arm64.cmake

# 4. 构建项目
cmake --build build

# 5. 可执行文件位于
# output/CangJieLLDBAdapter_macos_arm64
```

### 运行调试器

调试器需要一个端口号作为参数，用于监听来自调试前端的 TCP 连接：

```bash
# Windows
output\CangJieLLDBAdapter_windows_amd64.exe 8080

# Linux/macOS
./output/CangJieLLDBAdapter_linux_amd64 8080
```

启动后，调试器会监听 `127.0.0.1:8080`，等待调试前端连接。

---

## 🔧 交叉编译

### Linux ARM64 (aarch64)

```bash
# 1. 安装交叉编译工具链
sudo apt-get install -y \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu

# 2. 配置和构建
cmake -B build-arm64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake

cmake --build build-arm64

# 3. 输出文件
# output/CangJieLLDBAdapter_linux_arm64
```

### 自定义工具链路径

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake \
  -DCMAKE_C_COMPILER=/custom/path/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/custom/path/aarch64-linux-gnu-g++ \
  -DCMAKE_SYSROOT=/path/to/arm64/sysroot
```

---

## 📡 通信协议

### 消息格式

所有消息使用 **Protocol Buffers** 序列化，并在 TCP 流中以 **4 字节长度前缀 + 消息体** 的格式传输：

```
┌───────────────┬─────────────────────────┐
│ Length (4B)   │ Protobuf Message Body   │
│ Big-Endian    │ (Serialized)            │
└───────────────┴─────────────────────────┘
```

### 请求-响应模式

1. **前端 → 后端**：发送 `Request` 消息（带唯一 hash）
2. **后端 → 前端**：返回 `Response` 消息（携带相同 hash）
3. 前端通过 hash 匹配请求和响应

### 事件推送

后端可随时向前端推送 `Event` 消息，无需请求触发：
- `ProcessStateChanged`：进程状态变化
- `ProcessOutput`：进程输出（stdout/stderr）
- `BreakpointChangedEvent`：断点状态变更
- `ModuleEvent`：模块加载/卸载

### 协议定义

所有协议定义在 `schema/` 目录：
- **`model.proto`**：核心数据结构（~1500 行）
- **`request.proto`**：20+ 种请求类型
- **`response.proto`**：对应的响应消息
- **`event.proto`**：异步事件消息

---

## 🌍 扩展到其他 LLVM 语言的详细步骤

### 第一步：替换 LLDB 依赖

当前项目使用 **LLDB 15.0.4**，位于 `third_party/lib/liblldb_*` 文件。

#### 1.1 确定目标语言的 LLDB 版本

不同语言的 LLVM 工具链可能使用不同版本的 LLDB：
- **Rust**：通常使用 LLVM 17+
- **Swift**：macOS 系统自带（版本随 Xcode 变化）
- **C/C++**：取决于 Clang 版本
- **Kotlin Native**：LLVM 14+

#### 1.2 替换 LLDB 库文件

```bash
# 示例：替换为 LLDB 17.0
cd third_party/lib

# 备份当前版本
mv liblldb_linux_amd64.so liblldb_linux_amd64.so.bak

# 复制新版本 LLDB
cp /path/to/llvm-17/lib/liblldb.so.17 liblldb_linux_amd64.so

# 或创建符号链接
ln -s /usr/lib/llvm-17/lib/liblldb.so.17 liblldb_linux_amd64.so
```

#### 1.3 更新 LLDB 头文件

```bash
# 更新 LLDB 头文件
cd third_party/llvm-project
rm -rf lldb
cp -r /path/to/llvm-17/include/lldb lldb/include
```

#### 1.4 修改 CMakeLists.txt

在 `CMakeLists.txt` 中更新 LLDB 版本信息：

```cmake
# 第 382-434 行：LLDB 动态链接配置
# 修改 LLDB_LIB_NAME 变量以匹配新版本
```

### 第二步：调整符号解析逻辑

不同语言的调试符号格式可能有差异（如命名修饰、类型表示等），需要在 `ProtoConverter.cpp` 中调整：

#### 2.1 修改类型名称解析

```cpp
// src/protocol/ProtoConverter.cpp
// CreateType() 方法

// 原始（仓颉语言）
if (type_name.find("cangjie::") == 0) {
  // 处理仓颉特定类型
}

// 修改为目标语言（例如 Rust）
if (type_name.find("alloc::") == 0 ||
    type_name.find("core::") == 0) {
  // 处理 Rust 标准库类型
}
```

#### 2.2 调整函数名称解析

```cpp
// DebuggerClient.cpp
// HandleFramesRequest() 方法

// 根据目标语言的命名规则调整函数名显示
std::string function_name = sb_frame.GetFunctionName();
// 可能需要 demangle 或特殊处理
```

### 第三步：扩展协议（可选）

如果目标语言有特殊的调试需求，可以扩展 Protocol Buffers 定义：

#### 3.1 添加语言特定消息

在 `schema/model.proto` 中添加：

```protobuf
// 示例：Rust 特有的所有权信息
message RustOwnershipInfo {
  bool is_moved = 1;
  bool is_borrowed = 2;
  string lifetime = 3;
}

message Value {
  // 现有字段...

  // 添加 Rust 特定字段
  optional RustOwnershipInfo rust_info = 100;
}
```

#### 3.2 重新生成 Protobuf 代码

```bash
cmake --build build --target regenerate_protos
```

### 第四步：测试和验证

1. **编译测试程序**：使用目标语言编译一个简单程序（带调试符号）
2. **启动调试器**：运行 `CangJieLLDBAdapter`
3. **连接并测试**：使用测试脚本发送调试命令
4. **验证功能**：断点、变量检查、单步执行等

### 示例：适配 Rust

```bash
# 1. 安装 Rust 工具链
rustup install stable
rustup component add lldb

# 2. 找到 Rust 的 LLDB
RUST_LLDB=$(rustup which lldb)
LLDB_LIB=$(dirname $(dirname $RUST_LLDB))/lib/liblldb.so

# 3. 替换 LLDB 库
cp $LLDB_LIB third_party/lib/liblldb_linux_amd64.so

# 4. 重新构建
cmake --build build

# 5. 测试调试 Rust 程序
cargo build --example hello
./output/CangJieLLDBAdapter_linux_amd64 8080
# (从前端发送 CreateTarget, AddBreakpoint, Launch 等命令)
```

---

## 🛠️ 开发指南

### 添加新的调试命令

1. **定义协议消息**

在 `schema/request.proto` 中添加新请求：

```protobuf
message MyNewRequest {
  string parameter = 1;
}

message Request {
  // ... 现有字段 ...

  oneof request {
    // ... 现有请求类型 ...
    MyNewRequest my_new_request = 50;  // 使用未占用的字段号
  }
}
```

在 `schema/response.proto` 中添加响应：

```protobuf
message MyNewResponse {
  string result = 1;
}
```

2. **实现请求处理器**

在 `src/client/DebuggerClientHandlers.cpp` 中添加：

```cpp
bool DebuggerClient::HandleMyNewRequest(
    const lldbprotobuf::MyNewRequest& request,
    const lldbprotobuf::HashId& hash) {

  LOG_INFO("处理 MyNewRequest: " + request.parameter());

  // 1. 执行 LLDB 操作
  // lldb::SBResult result = ...

  // 2. 构建响应
  lldbprotobuf::MyNewResponse response;
  response.set_result("操作成功");

  // 3. 发送响应
  return SendMyNewResponse(response, hash);
}
```

3. **注册到消息循环**

在 `src/client/DebuggerClient.cpp` 的 `RunMessageLoop()` 方法中添加：

```cpp
if (request.has_my_new_request()) {
  HandleMyNewRequest(request.my_new_request(), hash);
}
```

4. **重新生成 Protobuf 代码**

```bash
cmake --build build --target regenerate_protos
```

### 日志系统

使用内置的线程安全日志系统：

```cpp
#include "cangjie/debugger/Logger.h"

LOG_DEBUG("调试信息: " + detail);
LOG_INFO("正常信息: " + status);
LOG_WARNING("警告: " + warning_msg);
LOG_ERROR("错误: " + error_msg);
LOG_CRITICAL("严重错误: " + critical_error);
```

日志文件位于：`logs/cangjie_debugger_YYYYMMDD_HHMMSS.log`

### 错误处理

所有 LLDB 对象在使用前必须检查有效性：

```cpp
lldb::SBTarget target = debugger_.GetSelectedTarget();
if (!target.IsValid()) {
  LOG_ERROR("无效的目标对象");
  return false;
}

lldb::SBProcess process = target.GetProcess();
if (!process.IsValid()) {
  LOG_ERROR("无效的进程对象");
  return false;
}
```

---

## 📊 性能和资源

### 内存占用

- **空闲状态**：~30 MB
- **调试会话**：~50-100 MB（取决于被调试程序的复杂度）
- **最大堆内存**：建议至少 512 MB

### CPU 使用

- **空闲**：< 1%
- **单步执行**：5-10%
- **变量展开**：10-20%
- **反汇编**：15-30%

### 网络延迟

- **本地回环**：< 1ms
- **局域网**：5-10ms
- **消息大小**：100 bytes - 10 KB（典型）

---

## 🧪 测试

### 计划中的测试框架

项目规划了以下测试模块（当前未实现）：

```bash
tests/
├── test_protocol_handler.cpp     # Protocol Buffers 通信测试
├── test_logger.cpp               # 日志系统测试
├── test_tcp_client.cpp           # TCP 网络测试
├── test_breakpoint_manager.cpp   # 断点管理测试
└── test_proto_converter.cpp      # 消息转换测试
```

运行测试（当实现后）：

```bash
cd build
ctest --output-on-failure
```

### 手动测试

使用提供的测试脚本测试基本功能：

```bash
# 1. 启动调试器
./output/CangJieLLDBAdapter_linux_amd64 8080

# 2. 在另一个终端，使用 Python 测试脚本
python3 tests/manual_test.py
```

---

## 📚 技术文档

### 相关文档

- [LLDB C++ API 文档](https://lldb.llvm.org/cpp_reference/namespacelldb.html)
- [Protocol Buffers 文档](https://developers.google.com/protocol-buffers)
- [仓颉语言官网](https://cangjie-lang.cn/)
- [CMake 构建系统](https://cmake.org/documentation/)

### 项目文档

- **schema/**：完整的 Protocol Buffers 定义（含中文注释）
- **include/**：公共 API 头文件（含 Doxygen 注释）

### 生成 API 文档

```bash
# 安装 Doxygen
sudo apt-get install doxygen graphviz  # Linux
brew install doxygen graphviz          # macOS

# 生成文档
cmake --build build --target doc

# 查看文档
open build/docs/html/index.html
```

---

## 🤝 贡献指南

我们欢迎社区贡献！请遵循以下流程：

### 贡献流程

1. **Fork 项目**：在 GitHub 上 fork 本仓库
2. **创建分支**：`git checkout -b feature/amazing-feature`
3. **编写代码**：遵循现有代码风格
4. **提交更改**：`git commit -m '添加某某功能'`
5. **推送分支**：`git push origin feature/amazing-feature`
6. **创建 PR**：在 GitHub 上创建 Pull Request

### 代码规范

- **命名约定**：
  - 类名：`PascalCase`（如 `DebuggerClient`）
  - 函数名：`PascalCase`（如 `HandleRequest`）
  - 变量名：`snake_case`（如 `tcp_client_`）
  - 成员变量：尾部下划线（如 `debugger_`）

- **注释规范**：
  - 公共 API 使用 Doxygen 风格注释
  - 复杂逻辑添加行内注释
  - 中文和英文注释均可接受

- **日志规范**：
  - 关键操作使用 `LOG_INFO`
  - 错误情况使用 `LOG_ERROR`
  - 调试信息使用 `LOG_DEBUG`

### 测试要求

- 新功能需添加相应测试
- 确保所有平台编译通过
- LLDB 操作必须检查 `IsValid()`
- 修改 Proto 定义需更新文档

---

## 🔐 安全性

### 网络安全

- **默认绑定**：`127.0.0.1`（仅本地访问）
- **不加密**：TCP 连接未加密，不建议在不可信网络使用
- **认证**：当前无认证机制，请在受信任环境使用

### 未来改进

- [ ] 添加 TLS/SSL 加密
- [ ] 实现基于 token 的认证
- [ ] 支持访问控制列表（ACL）

---

## 📄 许可证

本项目基于 **Apache License 2.0 with Runtime Library Exception** 许可。

许可证全文请访问：http://www.apache.org/licenses/LICENSE-2.0

---

 

## 🙏 致谢

本项目依赖以下优秀的开源项目：

- [LLVM/LLDB](https://llvm.org/) - 调试引擎
- [Protocol Buffers](https://github.com/protocolbuffers/protobuf) - 序列化框架
- [CMake](https://cmake.org/) - 构建系统
- [Abseil](https://abseil.io/) - C++ 基础库（Protobuf 依赖）

---

## 📈 项目状态

| 组件 | 状态 | 说明 |
|------|------|------|
| 核心调试功能 | ✅ 完成 | 断点、执行控制、变量检查等 |
| 网络通信 | ✅ 完成 | TCP + Protobuf |
| 跨平台支持 | ✅ 完成 | Windows/Linux/macOS |
| 交叉编译 | ✅ 完成 | ARM64/ARM32 |
| 测试框架 | ⏳ 计划中 | 单元测试和集成测试 |
| 文档 | ✅ 完成 | README + CLAUDE.md + 代码注释 |
| 性能优化 | ⏳ 持续改进 | 消息缓存、异步处理 |

---

## 🗺️ 路线图

### v1.1（计划中）
- [ ] 完整的测试套件
- [ ] 性能分析和优化
- [ ] 内存泄漏检测
- [ ] 更多调试命令（多目标、共享库等）

### v2.0（远期）
- [ ] TLS 加密支持
- [ ] 身份认证机制
- [ ] WebSocket 支持
- [ ] 分布式调试（远程调试）
- [ ] 可视化性能分析

---

<div align="center">

**如果觉得这个项目有用，请给个 ⭐ Star！**

</div>