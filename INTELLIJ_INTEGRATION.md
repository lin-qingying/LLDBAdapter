# Cangjie Debugger IntelliJ Integration Guide

本指南详细介绍如何将 Cangjie 调试器适配器集成到 IntelliJ IDEA 中，为仓颉语言提供完整的调试体验。

## 概述

Cangjie Debug Adapter 是一个实现了 **Debug Adapter Protocol (DAP)** 的调试器适配器，专门为 IntelliJ IDEA 和其他支持 DAP 的 IDE 提供仓颉语言调试功能。

### 主要特性

- **标准 DAP 支持**: 完全兼容 Microsoft Debug Adapter Protocol
- **IntelliJ 集成**: 无缝集成到 IntelliJ IDEA 的调试界面
- **动态 LLDB 加载**: 运行时加载 LLDB，无需编译时依赖
- **跨平台支持**: 支持 Windows、Linux、macOS
- **完整的调试功能**: 断点、单步执行、变量检查、表达式求值等

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

### 组件说明

1. **IntelliJ IDEA**: 作为调试前端，提供用户界面
2. **Debug Adapter**: 实现 DAP 协议，桥接 IntelliJ 和 LLDB
3. **LLDB**: 底层调试引擎，提供实际的调试能力
4. **仓颉程序**: 被调试的目标程序

## 安装和配置

### 1. 构建调试器适配器

```bash
# 克隆项目
git clone <repository-url>
cd cangjie_debugger

# 构建项目
python scripts/build.py --build-type Release --enable-tests

# 构建完成后，调试器适配器位于：
# Windows: build/Release/cangjie_debug_adapter.exe
# Linux/macOS: build/cangjie_debug_adapter
```

### 2. 安装依赖

#### Windows

```powershell
# 安装 LLDB
# 选项 1: 使用 vcpkg
vcpkg install llvm:x64-windows

# 选项 2: 手动下载
# 从 https://releases.llvm.org/ 下载 LLVM 并复制以下文件到 third_party/ 目录：
# - liblldb.dll
# - libclang.dll
# - 其他相关 DLL 文件
```

#### Linux

```bash
# Ubuntu/Debian
sudo apt-get install lldb-14 liblldb-14-dev

# CentOS/RHEL
sudo yum install lldb-devel

# 或者从源码编译 LLDB
```

#### macOS

```bash
# 使用 Homebrew
brew install llvm

# 或者使用 Xcode Command Line Tools
xcode-select --install
```

### 3. 配置 IntelliJ IDEA

#### 方法 1: 使用插件配置

1. 安装支持 DAP 的 IntelliJ 插件（如 "Code With Me" 或自定义调试插件）
2. 在插件设置中配置调试器适配器路径：
   ```
   调试器路径: /path/to/cangjie_debug_adapter
   LLDB 路径: /path/to/liblldb.so
   ```

#### 方法 2: 手动配置运行配置

创建 `.idea/runConfigurations/Debug Cangjie.xml`:

```xml
<component name="ProjectRunConfigurationManager">
  <configuration default="false" name="Debug Cangjie Program" type="CangjieDebugConfiguration" factoryName="Cangjie Debug">
    <option name="debuggerPath" value="$PROJECT_DIR$/../build/cangjie_debug_adapter" />
    <option name="program" value="$PROJECT_DIR$/target/program.cj" />
    <option name="arguments" value="" />
    <option name="workingDirectory" value="$PROJECT_DIR$" />
    <option name="stopAtEntry" value="false" />
    <option name="lldbPath" value="/usr/lib/liblldb.so" />
    <option name="sourcePaths">
      <list>
        <option value="$PROJECT_DIR$/src" />
      </list>
    </option>
    <option name="logLevel" value="INFO" />
    <option name="logFile" value="$PROJECT_DIR$/debug.log" />
    <method v="2">
      <option name="Make" enabled="true" />
    </method>
  </configuration>
</component>
```

## 使用指南

### 启动调试会话

#### 方式 1: 使用启动脚本

**Windows:**
```batch
# 基本启动
scripts\start_debug_adapter.bat

# 指定 LLDB 路径
scripts\start_debug_adapter.bat --lldb-path C:\LLVM\bin\liblldb.dll

# 启用调试日志
scripts\start_debug_adapter.bat --log-level DEBUG --log-file debug.log

# TCP 模式（用于远程调试）
scripts\start_debug_adapter.bat --port 4711
```

**Linux/macOS:**
```bash
# 基本启动
./scripts/start_debug_adapter.sh

# 指定 LLDB 路径
./scripts/start_debug_adapter.sh --lldb-path /usr/lib/liblldb.so

# 启用调试日志
./scripts/start_debug_adapter.sh --log-level DEBUG --log-file debug.log

# TCP 模式（用于远程调试）
./scripts/start_debug_adapter.sh --port 4711
```

#### 方式 2: 直接运行

```bash
# stdio 模式（默认，用于本地调试）
./cangjie_debug_adapter --lldb-path /usr/lib/liblldb.so

# TCP 模式（用于远程调试）
./cangjie_debug_adapter --lldb-path /usr/lib/liblldb.so --port 4711

# 启用详细日志
./cangjie_debug_adapter --lldb-path /usr/lib/liblldb.so --log-level DEBUG --log-file debug.log
```

### IntelliJ IDEA 中的调试操作

一旦配置完成，您可以在 IntelliJ IDEA 中执行以下调试操作：

#### 1. 设置断点

- **行断点**: 在源代码行号左侧点击设置断点
- **函数断点**: 在函数名上右键选择 "Add Function Breakpoint"
- **条件断点**: 右键断点设置条件表达式
- **日志断点**: 断点触发时输出日志而不暂停

#### 2. 执行控制

- **继续 (F9)**: 继续执行程序
- **单步跳过 (F8)**: 执行下一行，不进入函数
- **单步进入 (F7)**: 进入函数内部
- **单步跳出 (Shift+F8)**: 执行到函数返回
- **暂停 (Ctrl+F2)**: 暂停正在执行的程序

#### 3. 变量检查

- **局部变量**: 在 "Variables" 窗口查看当前作用域的变量
- **监视表达式**: 添加自定义监视表达式
- **求值**: 在 "Evaluate Expression" 窗口求值表达式
- **内存视图**: 查看内存地址的内容

#### 4. 调用栈

- **调用栈窗口**: 查看完整的函数调用链
- **帧切换**: 在调用栈的不同帧之间切换
- **源码导航**: 双击栈帧跳转到对应源码位置

## 调试配置选项

### 启动配置 (Launch)

```json
{
  "type": "cangjie",
  "request": "launch",
  "name": "Debug Cangjie Program",
  "program": "${workspaceFolder}/target/program.cj",
  "args": ["arg1", "arg2"],
  "cwd": "${workspaceFolder}",
  "environment": [
    {
      "name": "PATH",
      "value": "/usr/local/bin:/usr/bin"
    }
  ],
  "stopAtEntry": false,
  "lldbPath": "/usr/lib/liblldb.so",
  "sourcePaths": ["${workspaceFolder}/src"],
  "logLevel": "INFO",
  "logFile": "debug.log"
}
```

### 附加配置 (Attach)

```json
{
  "type": "cangjie",
  "request": "attach",
  "name": "Attach to Process",
  "processId": 12345,
  "lldbPath": "/usr/lib/liblldb.so",
  "logLevel": "INFO"
}
```

### 配置参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `program` | string | 要调试的程序路径 |
| `args` | array | 程序启动参数 |
| `cwd` | string | 工作目录 |
| `environment` | array | 环境变量 |
| `stopAtEntry` | boolean | 是否在程序入口停止 |
| `lldbPath` | string | LLDB 库路径 |
| `sourcePaths` | array | 源码搜索路径 |
| `logLevel` | string | 日志级别 (DEBUG/INFO/WARNING/ERROR/CRITICAL) |
| `logFile` | string | 日志文件路径 |

## 远程调试

### 设置远程调试

1. **在远程机器上启动调试器适配器**:
   ```bash
   ./cangjie_debug_adapter --lldb-path /usr/lib/liblldb.so --port 4711
   ```

2. **在 IntelliJ 中配置远程调试**:
   - 设置调试器地址为 `remote-host:4711`
   - 确保网络连接正常
   - 配置源码映射

3. **开始远程调试会话**

### SSH 隧道调试

```bash
# 创建 SSH 隧道
ssh -L 4711:localhost:4711 user@remote-machine

# 在 IntelliJ 中连接到 localhost:4711
```

## 故障排除

### 常见问题

#### 1. 调试器启动失败

**症状**: 无法启动调试器适配器
**解决方案**:
```bash
# 检查 LLDB 是否正确安装
lldb --version

# 检查调试器适配器是否存在
ls -la build/cangjie_debug_adapter

# 查看详细错误日志
./cangjie_debug_adapter --log-level DEBUG --log-file debug.log
```

#### 2. 断点不生效

**症状**: 设置的断点没有被触发
**解决方案**:
- 确保程序使用调试模式编译 (`-g` 标志)
- 检查源码路径是否正确
- 验证可执行文件是否包含调试信息

#### 3. 无法连接到调试器

**症状**: IntelliJ 无法连接到调试器适配器
**解决方案**:
- 检查调试器适配器是否正在运行
- 验证端口是否被占用
- 检查防火墙设置

#### 4. 变量显示不正确

**症状**: 变量值显示错误或不显示
**解决方案**:
- 确保程序在调试模式下编译
- 检查优化级别 (`-O0` 禁用优化)
- 验证调试信息格式

### 调试日志

启用详细日志来诊断问题：

```bash
# 启用调试日志
./cangjie_debug_adapter --log-level DEBUG --log-file debug.log

# 查看日志内容
tail -f debug.log
```

日志内容包括：
- DAP 协议通信
- LLDB 操作
- 错误信息
- 性能统计

### 性能优化

#### 1. 减少调试信息

```bash
# 仅在需要时启用调试日志
--log-level ERROR

# 限制日志文件大小
--log-file /dev/null
```

#### 2. 优化源码搜索

```json
{
  "sourcePaths": [
    "${workspaceFolder}/src",
    "${workspaceFolder}/include"
  ]
}
```

#### 3. 禁用不必要的功能

```json
{
  "enableVariableHover": false,
  "enableConditionalBreakpoints": false
}
```

## 高级功能

### 1. 自定义调试器扩展

```cpp
// 扩展调试器功能
class CustomDebugAdapter : public DebugAdapter {
public:
    // 自定义命令处理
    DAP::Response HandleCustomCommand(const DAP::Request& request) override {
        // 实现自定义调试功能
    }
};
```

### 2. 条件断点

```cpp
// 在 IntelliJ 中设置条件断点
// 条件表达式: i > 10 && i < 20
// 日志断点: printf("i = %d\n", i)
```

### 3. 异常处理

```json
{
  "exceptionBreakpointFilters": [
    {
      "filter": "all",
      "label": "All Exceptions",
      "default": false
    },
    {
      "filter": "uncaught",
      "label": "Uncaught Exceptions",
      "default": true
    }
  ]
}
```

### 4. 内存调试

```bash
# 在调试器中查看内存
# 命令: memory view 0x7ffff7dd6000 100
# 地址: 0x7ffff7dd6000
# 大小: 100 字节
```

## 最佳实践

### 1. 开发工作流

1. **编写代码** → 2. **编译调试版本** → 3. **设置断点** → 4. **启动调试** → 5. **分析问题**

### 2. 调试技巧

- **使用条件断点**减少不必要的停止
- **设置监视表达式**跟踪变量变化
- **利用调用栈**理解程序执行流程
- **使用内存视图**检查数据结构

### 3. 性能考虑

- 避免在循环中设置断点
- 合理使用日志断点
- 限制源码搜索范围
- 及时清理不需要的监视表达式

## 支持和贡献

### 获取帮助

- **文档**: 查看项目文档和 API 参考
- **示例**: 参考 examples/ 目录中的示例
- **日志**: 分析调试日志诊断问题
- **社区**: 在项目仓库提交 Issue

### 贡献代码

1. Fork 项目仓库
2. 创建功能分支
3. 实现新功能或修复问题
4. 添加测试用例
5. 提交 Pull Request

### 报告问题

提交 Issue 时请包含：
- 操作系统和版本
- IntelliJ IDEA 版本
- 调试器适配器版本
- 重现步骤
- 错误日志
- 预期行为

---

通过本指南，您应该能够成功地将 Cangjie 调试器集成到 IntelliJ IDEA 中，享受完整的仓颉语言调试体验。