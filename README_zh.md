# 仓颉调试器 (Cangjie Debugger)

专为仓颉编程语言设计的高性能调试器，使用 LLDB 作为调试后端，Protocol Buffers 进行高效通信。

## 🚀 快速开始

### 前置要求

- **编译器**: 支持 C++20 的编译器 (GCC 10+, Clang 12+, MSVC 2022+)
- **CMake**: 版本 3.16.5 或更高
- **LLVM/LLDB**: 调试器后端（自动处理）
- **Protocol Buffers**: v3 版本（自动从源码构建）

### 快速构建

```bash
# 克隆仓库
git clone <repository-url>
cd cangjie_debugger

# 使用 CMake 预设构建（推荐）
cmake --preset windows-default    # Windows with MSVC
# cmake --preset windows-mingw    # Windows with MinGW
# cmake --preset linux-default    # Linux

cmake --build --preset windows-release
```

### 替代构建方法

```bash
# 传统 CMake 构建
mkdir build && cd build
cmake ..
cmake --build .

# 或在 Unix 系统上
make
```

主可执行文件将位于：
- **Windows**: `output/CangJieLLDBFrontend.exe`
- **Linux/macOS**: `output/CangJieLLDBFrontend`

### 运行调试器

调试器需要端口号进行 TCP 通信：

```bash
# 在端口 8080 启动调试器前端
output/CangJieLLDBFrontend.exe 8080  # Windows
./output/CangJieLLDBFrontend 8080     # Linux/macOS
```

## 📁 项目结构

```
cangjie_debugger/
├── schema/                     # Protocol Buffers 定义文件
│   ├── model.proto            # 核心数据结构
│   ├── request.proto          # 请求消息（20+ 种类型）
│   ├── response.proto         # 响应消息
│   └── event.proto           # 事件/广播消息
├── src/                       # 源代码
│   ├── client/               # 客户端网络和调试功能
│   │   ├── DebuggerClient.cpp # 主调试器客户端
│   │   └── TcpClient.cpp     # TCP 通信层
│   ├── core/                 # 核心调试功能
│   │   └── BreakpointManager.cpp # 断点管理
│   ├── protocol/             # 协议处理
│   │   └── ProtoConverter.cpp # Protobuf 转换工具
│   ├── utils/                # 工具函数
│   │   └── Logger.cpp        # 线程安全日志系统
│   └── main.cpp              # 应用程序入口点
├── include/                   # 头文件
│   └── cangjie/debugger/     # 公共 API 头文件
├── cmake/                     # CMake 模块和预设
├── third_party/              # 依赖项（protobuf, llvm-project）
├── CMakePresets.json         # 不同平台的构建预设
└── tests/                    # 测试文件（计划中）
```

## 🔧 Protocol Buffers

### 自动生成

本项目使用 Protocol Buffers v3 和专为 LLDB 优化的自定义协议。Proto 文件会自动生成：

1. **配置期间**: 运行 `cmake ..` 时
2. **文件变更时**: 当 `.proto` 文件被修改时
3. **手动重新生成**:
   ```bash
   cmake --build . --target regenerate_protoids
   ```

### 生成文件位置

```
build/generated/proto/
├── model.pb.h/.cc
├── request.pb.h/.cc
├── response.pb.h/.cc
└── event.pb.h/.cc
```

## 🎯 特性

- ✅ **Protocol Buffers v3**: 专为 LLDB 集成优化的自定义协议
- ✅ **LLDB 集成**: 与 liblldb 的动态运行时链接
- ✅ **跨平台支持**: Windows (MSVC/MinGW), Linux, macOS
- ✅ **自动 Proto 生成**: 无缝开发工作流
- ✅ **事件驱动架构**: 异步事件处理
- ✅ **TCP 通信**: 基于网络的调试协议
- ✅ **全面调试功能**: 断点、单步执行、变量检查、内存查看

## 🏗️ 架构设计

调试器采用分层架构：

```
调试前端（IDE/编辑器）
    ↓ (TCP + Protocol Buffers)
CangJieLLDBFrontend（主可执行文件）
    ↓ (动态库加载)
liblldb.dll / liblldb.so / liblldb.dylib
    ↓
目标仓颉程序进程
```

### 核心组件

1. **LLDB 集成层** (`src/client/`)
   - `DebuggerClient.cpp`: 处理协议消息的主调试器接口
   - `TcpClient.cpp`: 使用 protobuf 消息进行网络通信
   - 动态 LLDB 库加载，实现跨平台兼容性

2. **协议通信层** (`src/protocol/`)
   - `ProtoConverter.cpp`: 在 protobuf 消息和 LLDB 对象之间进行转换
   - 带有中文注释的自定义 protobuf 架构
   - 高效的二进制序列化用于网络通信

3. **调试管理层** (`src/core/`)
   - `BreakpointManager.cpp`: 管理行断点、地址断点、函数断点和监视点
   - 线程状态管理和枚举
   - 变量求值和检查

4. **工具层** (`src/utils/`)
   - `Logger.cpp`: 具有多级别的线程安全日志系统
   - 平台抽象和错误处理

## 🔧 开发工作流

### 添加新的调试命令

1. 在适当的 `.proto` 文件中定义消息（`request.proto`、`response.proto` 或 `event.proto`）
2. 在 `DebuggerClient::Handle*Request()` 中添加处理器
3. 在 `DebuggerClient::Send*Response()` 中添加响应发送器
4. 为任何新数据类型更新 `ProtoConverter`

### Protocol Buffer 更改

1. 编辑 `schema/` 目录中的 `.proto` 文件
2. CMake 在下次构建配置时自动重新生成
3. 从 `build/generated/proto/` 包含生成的头文件
4. 使用 `cmake --build . --target regenerate_protoids` 测试重新生成

### 构建目标

```bash
# 仅构建主可执行文件
cmake --build . --target CangJieLLDBFrontend

# 手动重新生成 protobuf 文件
cmake --build . --target regenerate_protoids

# 从源码重新构建 protobuf
cmake --build . --target rebuild_protobuf
```

## 🧪 测试

测试基础设施已规划但尚未实现。实现后：

```bash
cd build
ctest  # 运行所有测试

# 单独测试组件（计划中）
./tests/test_protocol_handler
./tests/test_logger
./tests/test_tcp_client
./tests/test_breakpoint_manager
./tests/test_proto_converter
```

## 🐛 故障排除

### 构建问题

**Protobuf 生成错误**:
```bash
# 检查 protoc 可用性
protoc --version

# 强制重新生成
cmake --build . --target regenerate_protoids
```

**找不到 LLDB**:
- 确保系统上安装了 LLDB
- 检查 `third_party/` 目录中的 liblldb 文件
- Windows 用户应在 `third_party/` 中有 `liblldb.dll`

**编译错误**:
- 验证 C++20 编译器支持
- 检查 CMake 版本（3.16.5+）
- 确保所有依赖项都在 `third_party/` 中

### 运行时问题

**连接失败**:
- 验证端口号有效（1-65535）
- 检查防火墙设置
- 确保没有其他进程使用相同端口

**LLDB 初始化失败**:
- 检查 LLDB 安装
- 验证 liblldb 库兼容性
- 启用调试日志获取详细错误信息

### 调试日志

启用详细日志以解决问题：

```cpp
// 在 main.cpp 中，修改日志级别
Cangjie::Debugger::Logger::Initialize("cangjie_debugger.log",
                                     Cangjie::Debugger::LogLevel::DEBUG, true);
```

## 📚 API 文档

项目使用带有详细中文注释的自定义 Protocol Buffers 架构。关键消息类型：

- **请求消息** (`request.proto`): 20+ 请求类型，包括 CreateTarget、Launch、Attach、Continue、StepInto、StepOver、StepOut、AddBreakpoint、Variables、Evaluate 等
- **数据模型** (`model.proto`): Thread、Frame、Variable、Breakpoint、ProcessInfo、SourceLocation
- **事件** (`event.proto`): ProcessStopped、ProcessExited、ModuleLoaded、BreakpointChanged 等

## 🤝 贡献

我们欢迎社区贡献！请遵循以下步骤：

1. Fork 项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 开发指南

- 遵循现有的代码风格和命名约定
- 使用 `LOG_INFO()`、`LOG_ERROR()` 等宏添加适当的日志
- 确保所有 LLDB 操作在使用前检查 `IsValid()`
- 在进行平台特定更改时跨平台测试
- 添加新协议消息时更新 protobuf 架构并重新生成

## 📄 许可证

本项目基于 Apache License 2.0 并附带 Runtime Library Exception 许可。详情请参阅 [LICENSE](LICENSE) 文件。

## 🔗 相关链接

- [LLDB 官方文档](https://lldb.llvm.org/)
- [Protocol Buffers 文档](https://developers.google.com/protocol-buffers)
- [仓颉语言官网](https://cangjie-lang.cn/)
- [CMake 预设文档](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

---

**注意**: 这是仓颉调试器的 LLDB 前端。它使用自定义 protobuf 协议通过 TCP 通信，需要单独的调试前端或 IDE 集成来提供完整的调试体验。