# Cangjie Debugger

仓颉调试器项目，支持基于 Protocol Buffers 的调试协议。

## 🚀 快速开始

### 前置要求

1. **编译器**: 支持 C++17 的编译器 (GCC 7+, Clang 5+, MSVC 2019+)
2. **CMake**: 版本 3.16.5 或更高
3. **Protocol Buffers**: protoc 编译器和 C++ 库
4. **LLVM/LLDB**: 调试器支持 (可选，如不使用将动态链接)

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install cmake build-essential protobuf-compiler libprotobuf-dev liblldb-dev
```

**macOS:**
```bash
brew install cmake protobuf llvm
```

**Windows:**
```cmd
# 使用 WinGet
winget install Google.Protobuf
winget install LLVM.LLVM

# 或者手动下载安装包
```

## 🛠️ 构建项目

### 1. 配置项目

```bash
mkdir build && cd build
cmake ..
```

**重要**: 运行 `cmake ..` 时会自动编译所有 `.proto` 文件，生成的头文件和源文件位于：
```
build/generated/proto/
```

### 2. 编译项目

```bash
# Linux/macOS
make

# Windows (Visual Studio)
cmake --build . --config Debug
```

## 📁 项目结构

```
cangjie_debugger/
├── schema/              # Protocol Buffers 定义文件
│   ├── model.proto
│   ├── protocol.proto
│   ├── protocol_responses.proto
│   └── broadcasts.proto
├── src/                 # 源代码
│   ├── core/           # 核心调试功能
│   ├── lldb/           # LLDB 封装
│   ├── protocol/       # 协议处理
│   ├── server/         # 调试服务器
│   ├── client/         # 调试客户端
│   ├── adapter/        # IDE 适配器
│   └── utils/          # 工具类
├── include/            # 头文件
├── tests/              # 测试文件
└── build/generated/proto/  # 自动生成的 proto 文件
```

## 🔧 Protocol Buffers

### 自动编译

本项目使用 Protocol Buffers v3 语法。proto 文件在以下时机自动编译：

1. **配置时**: 运行 `cmake ..` 时自动编译
2. **重载时**: 在 IDE 中重新加载 CMake 项目时自动编译
3. **手动重新生成**:
   ```bash
   make regenerate_protos  # Linux/macOS
   cmake --build . --target regenerate_protos  # Windows
   ```

### 生成的文件位置

```
build/generated/proto/
├── model.pb.h/.cc
├── protocol.pb.h/.cc
├── protocol_responses.pb.h/.cc
└── broadcasts.pb.h/.cc
```

## 🎯 特性

- ✅ Protocol Buffers v3 支持
- ✅ LLDB 调试器集成
- ✅ 跨平台支持 (Windows, Linux, macOS)
- ✅ 自动 proto 文件编译
- ✅ 调试适配器协议支持
- ✅ 模块化架构

## 🐛 故障排除

### 找不到 protoc

确保 protoc 已安装并且在系统 PATH 中：
```bash
protoc --version
```

### CMake 找不到 Protobuf

确保安装了 Protobuf 开发包：
```bash
# 检查 CMake 能否找到 Protobuf
cmake --find-package -DNAME=Protobuf -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST
```

### 编译错误

1. 检查 proto 文件语法是否正确
2. 确保枚举的第一个值为 0
3. 检查导入路径是否正确

## 📚 API 文档

生成的 API 文档位于 `build/docs/html/`（如果安装了 Doxygen）。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目基于 Apache-2.0 许可证开源。

- **Debug Adapter Protocol 支持**: 完全兼容 Microsoft DAP 标准
- **IntelliJ IDEA 集成**: 无缝集成到 IntelliJ IDEA 的调试界面
- **动态 LLDB 加载**: 运行时动态加载 liblldb，无需编译时依赖
- **跨平台支持**: 支持 Windows、Linux、macOS
- **完整的调试功能**: 断点管理、单步执行、变量检查、表达式求值、内存查看等
- **远程调试**: 支持 TCP 模式的远程调试
- **高性能**: 基于 Protocol Buffers 的高效通信

## 快速开始

### 1. 构建调试器适配器

```bash
# 克隆项目
git clone <repository-url>
cd cangjie_debugger

# 构建项目
python scripts/build.py --build-type Release

# 构建完成后，调试器适配器位于：
# Windows: build/Release/cangjie_debug_adapter.exe
# Linux/macOS: build/cangjie_debug_adapter
```

### 2. 启动调试器适配器

**Windows:**
```batch
# 基本启动
scripts\start_debug_adapter.bat

# 指定 LLDB 路径
scripts\start_debug_adapter.bat --lldb-path C:\LLVM\bin\liblldb.dll
```

**Linux/macOS:**
```bash
# 基本启动
./scripts/start_debug_adapter.sh

# 指定 LLDB 路径
./scripts/start_debug_adapter.sh --lldb-path /usr/lib/liblldb.so
```

### 3. 在 IntelliJ IDEA 中配置

1. 安装支持 DAP 的 IntelliJ 插件
2. 配置调试器适配器路径：`/path/to/cangjie_debug_adapter`
3. 创建调试配置，指定要调试的仓颉程序
4. 开始调试！

## 项目结构

```
cangjie_debugger/
├── src/adapter/                # Debug Adapter 实现
│   ├── DebugAdapter.h          # DAP 适配器头文件
│   ├── DebugAdapter.cpp        # DAP 适配器实现
│   └── DebugAdapterMain.cpp    # 主程序入口
├── include/cangjie/debugger/   # 公共头文件
├── schema/                     # Protocol Buffers 定义
├── scripts/                    # 构建和启动脚本
├── examples/intellij/          # IntelliJ 配置示例
└── docs/                       # 文档
```

## 架构设计

```
IntelliJ IDEA
    ↓ (Debug Adapter Protocol)
Cangjie Debug Adapter
    ↓ (Dynamic Library Loading)
liblldb.dll / liblldb.so
    ↓
仓颉程序进程
```

## IntelliJ 集成

### 调试配置示例

```xml
<component name="ProjectRunConfigurationManager">
  <configuration name="Debug Cangjie Program" type="CangjieDebugConfiguration">
    <option name="debuggerPath" value="$PROJECT_DIR$/../build/cangjie_debug_adapter" />
    <option name="program" value="$PROJECT_DIR$/target/program.cj" />
    <option name="stopAtEntry" value="false" />
    <option name="lldbPath" value="/usr/lib/liblldb.so" />
    <option name="logLevel" value="INFO" />
  </configuration>
</component>
```

### 支持的调试功能

- **断点管理**: 行断点、函数断点、条件断点
- **执行控制**: 继续、单步跳过、单步进入、单步跳出
- **变量检查**: 局部变量、参数、监视表达式
- **表达式求值**: 动态求值任意表达式
- **调用栈**: 完整的函数调用链查看
- **内存查看**: 内存地址内容查看和修改
- **线程管理**: 多线程程序调试
- **异常处理**: 异常断点和异常信息查看

## 系统要求

### 最低要求

- **IntelliJ IDEA**: 2020.3 或更高版本（支持 DAP 插件）
- **操作系统**: Windows 10+, Ubuntu 18.04+, macOS 10.15+
- **LLDB**: 10.0 或更高版本
- **CMake**: 3.16.5 或更高版本
- **Python**: 3.6 或更高版本（用于构建脚本）

### 推荐配置

- **IntelliJ IDEA**: 2023.1 或更高版本
- **内存**: 8GB RAM 或更多
- **存储**: SSD 硬盘，至少 2GB 可用空间

## 安装依赖

### Windows

```powershell
# 安装 LLDB（使用 vcpkg）
vcpkg install llvm:x64-windows

# 或从官网下载 LLVM
# https://releases.llvm.org/
```

### Linux

```bash
# Ubuntu/Debian
sudo apt-get install lldb-14 liblldb-14-dev

# CentOS/RHEL
sudo yum install lldb-devel
```

### macOS

```bash
# 使用 Homebrew
brew install llvm

# 或使用 Xcode Command Line Tools
xcode-select --install
```

## 构建选项

```bash
# 基本构建
python scripts/build.py

# 发布版本构建
python scripts/build.py --build-type Release

# 启用测试
python scripts/build.py --enable-tests --run-tests

# 创建安装包
python scripts/build.py --package ZIP
```

## 使用指南

### 命令行选项

```bash
# 基本启动
./cangjie_debug_adapter

# 指定 LLDB 路径
./cangjie_debug_adapter --lldb-path /path/to/liblldb.so

# 设置日志级别
./cangjie_debug_adapter --log-level DEBUG

# TCP 模式（远程调试）
./cangjie_debug_adapter --port 4711

# 查看帮助
./cangjie_debug_adapter --help
```

### 远程调试设置

1. **在远程机器上启动调试器**:
   ```bash
   ./cangjie_debug_adapter --lldb-path /usr/lib/liblldb.so --port 4711
   ```

2. **在 IntelliJ 中配置远程调试**:
   - 设置调试器地址为 `remote-host:4711`
   - 配置源码映射
   - 开始远程调试会话

## 故障排除

### 常见问题

1. **调试器启动失败**
   ```bash
   # 检查 LLDB 安装
   lldb --version

   # 查看详细日志
   ./cangjie_debug_adapter --log-level DEBUG
   ```

2. **断点不生效**
   - 确保程序使用调试模式编译 (`-g` 标志)
   - 检查源码路径配置

3. **连接失败**
   - 验证调试器适配器是否正在运行
   - 检查防火墙设置

### 日志分析

```bash
# 启用详细日志
./cangjie_debug_adapter --log-level DEBUG --log-file debug.log

# 查看日志
tail -f debug.log
```

## 开发指南

### 添加新功能

1. 在 `DebugAdapter.h` 中声明新的处理方法
2. 在 `DebugAdapter.cpp` 中实现功能
3. 在 `DebugAdapterMain.cpp` 中注册命令处理器
4. 添加相应的测试用例

### 调试适配器本身

```bash
# 构建调试版本
python scripts/build.py --build-type Debug

# 启用调试日志
./cangjie_debug_adapter --log-level DEBUG
```

## 贡献

我们欢迎社区贡献！请遵循以下步骤：

1. Fork 项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 许可证

本项目基于 Apache-2.0 许可证，包含 Runtime Library Exception。详见 [LICENSE](LICENSE) 文件。

## 联系方式

- **问题报告**: [GitHub Issues](https://github.com/your-repo/cangjie-debugger/issues)
- **功能请求**: [GitHub Discussions](https://github.com/your-repo/cangjie-debugger/discussions)
- **邮件**: development@cangjie-lang.org

## 相关链接

- [Debug Adapter Protocol 规范](https://microsoft.github.io/debug-adapter-protocol/)
- [IntelliJ IDEA 调试文档](https://www.jetbrains.com/help/idea/debugging-code.html)
- [LLDB 官方文档](https://lldb.llvm.org/)
- [仓颉语言官网](https://cangjie-lang.cn/)

---

**注意**: 这是一个专门为 IntelliJ IDEA 设计的调试器适配器，如果您需要通用的调试器实现，请参考项目中的其他组件。