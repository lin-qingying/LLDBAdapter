# 优化后的Proto使用指南

## 概述

本文档介绍如何使用优化后的Protocol Buffers消息定义来实现仓颉调试器的通信协议。

## 主要改进

### 1. 命名规范优化

#### 枚举类型
- **旧命名**: `OutputTypeStdout`, `ThreadStopReasonBreakpoint`
- **新命名**: `OUTPUT_TYPE_STDOUT`, `STOP_REASON_BREAKPOINT`
- **优势**: 符合protobuf最佳实践，使用全大写下划线分隔

#### 消息类型
- **旧命名**: `CreateTarget_Req`, `Launch_Res`, `ProcessInterrupted_Broadcast`
- **新命名**: `CreateTargetRequest`, `LaunchResponse`, `ProcessInterruptedEvent`
- **优势**: 更清晰的语义，统一的后缀命名

#### 字段名称
- **旧命名**: `exe_path`, `to_wait`, `step_instruction`
- **新命名**: `executable_path`, `wait_for_launch`, `step_by_instruction`
- **优势**: 更具描述性，避免缩写

### 2. 类型结构优化

#### 提取公共结构
```protobuf
// 旧方式: 在多个消息中重复定义文件路径和行号
message AddBreakpoint_Req {
  string file = 2;
  int32 line = 3;
  HashType hash_type = 4;
  string hash = 5;
}

// 新方式: 使用统一的SourceLocation
message SourceLocation {
  string file_path = 1;
  uint32 line = 2;
  HashAlgorithm hash_algorithm = 3;
  string hash_value = 4;
}

message AddBreakpointRequest {
  string file_path = 1;
  uint32 line = 2;
  string condition = 3;
  // ... 其他字段
}
```

#### 逻辑分组
所有消息按功能分组，并添加清晰的注释：
- 基础类型 (Basic Types)
- 线程和执行状态 (Thread and Execution State)
- 值和类型 (Values and Types)
- 内存和反汇编 (Memory and Disassembly)
- 断点和观察点 (Breakpoints and Watchpoints)

## 使用示例

### 1. 创建请求消息

```cpp
#include "cangjie/debugger/ProtoConverter.h"

using namespace Cangjie::Debugger;

// 创建目标
auto create_target_req = ProtoConverter::CreateCreateTargetRequest(
    "/path/to/executable",
    "x86_64"
);

// 创建启动请求
std::vector<std::string> args = {"--verbose", "--debug"};
std::vector<std::pair<std::string, std::string>> env = {
    {"PATH", "/usr/bin"},
    {"DEBUG_MODE", "1"}
};

auto launch_info = ProtoConverter::CreateProcessLaunchInfo(
    "/path/to/executable",
    "/working/directory",
    args,
    env
);

auto launch_req = ProtoConverter::CreateLaunchRequest(
    launch_info,
    proto::CONSOLE_MODE_EXTERNAL
);

// 添加断点
auto bp_req = ProtoConverter::CreateAddBreakpointRequest(
    "main.cj",
    42,
    "x > 10"  // 条件断点
);

// 单步执行
auto step_into_req = ProtoConverter::CreateStepIntoRequest(
    thread_id,
    false  // 不按指令单步
);
```

### 2. 创建响应消息

```cpp
// 创建成功的启动响应
auto launch_resp = ProtoConverter::CreateLaunchResponse(
    true,           // 成功
    12345,          // 进程ID
    ""              // 无错误消息
);

// 创建失败的响应
auto error_resp = ProtoConverter::CreateLaunchResponse(
    false,
    0,
    "无法找到可执行文件"
);

// 创建线程列表响应
std::vector<proto::Thread> threads;
for (int i = 0; i < 3; i++) {
    auto stop_info = ProtoConverter::CreateThreadStopInfo(
        proto::STOP_REASON_BREAKPOINT,
        "命中断点"
    );

    auto thread = ProtoConverter::CreateThread(
        i,
        1000 + i,
        "Thread-" + std::to_string(i),
        stop_info
    );

    threads.push_back(thread);
}

auto threads_resp = ProtoConverter::CreateGetThreadsResponse(
    true,
    threads,
    ""
);
```

### 3. 创建事件消息

```cpp
// 进程中断事件
auto stop_info = ProtoConverter::CreateThreadStopInfo(
    proto::STOP_REASON_BREAKPOINT,
    "命中断点 #1"
);

auto thread = ProtoConverter::CreateThread(
    0,
    1001,
    "main",
    stop_info
);

auto location = ProtoConverter::CreateSourceLocation(
    "main.cj",
    42
);

auto frame = ProtoConverter::CreateStackFrame(
    0,
    "main",
    location,
    0x400500
);

auto interrupted_event = ProtoConverter::CreateProcessInterruptedEvent(
    thread,
    frame
);

// 进程输出事件
auto output_event = ProtoConverter::CreateProcessOutputEvent(
    "Hello, World!\n",
    proto::OUTPUT_TYPE_STDOUT
);

// 断点添加事件
auto breakpoint = ProtoConverter::CreateBreakpoint(
    1,
    location,
    "x > 10"
);

std::vector<proto::BreakpointLocation> locations;
auto bp_location = ProtoConverter::CreateBreakpointLocation(
    1,
    0x400500,
    true,
    location
);
locations.push_back(bp_location);

auto bp_added_event = ProtoConverter::CreateBreakpointAddedEvent(
    breakpoint,
    locations
);
```

### 4. 使用复合消息

```cpp
// 创建复合请求（批量请求）
proto::CompositeRequest composite_req;

// 添加多个请求
*composite_req.mutable_get_threads() =
    ProtoConverter::CreateGetThreadsRequest();

*composite_req.mutable_get_frames() =
    ProtoConverter::CreateGetFramesRequest(1, 0, 10);

// 序列化并发送
std::string serialized;
composite_req.SerializeToString(&serialized);
// ... 发送serialized数据

// 创建复合响应
proto::CompositeResponse composite_resp;

*composite_resp.mutable_get_threads() = threads_resp;
*composite_resp.mutable_get_frames() = frames_resp;
```

## 枚举值对照表

### 输出类型 (OutputType)
| 旧值 | 新值 | 说明 |
|------|------|------|
| OutputTypeStdout | OUTPUT_TYPE_STDOUT | 标准输出 |
| OutputTypeStderr | OUTPUT_TYPE_STDERR | 标准错误 |

### 停止原因 (StopReason)
| 旧值 | 新值 | 说明 |
|------|------|------|
| ThreadStopReasonInvalid | STOP_REASON_INVALID | 无效原因 |
| ThreadStopReasonBreakpoint | STOP_REASON_BREAKPOINT | 断点 |
| ThreadStopReasonWatchpoint | STOP_REASON_WATCHPOINT | 观察点 |
| ThreadStopReasonSignal | STOP_REASON_SIGNAL | 信号 |
| ThreadStopReasonException | STOP_REASON_EXCEPTION | 异常 |

### 控制台模式 (ConsoleMode)
| 旧值 | 新值 | 说明 |
|------|------|------|
| LaunchInParentConsole | CONSOLE_MODE_PARENT | 父控制台 |
| LaunchInExternalConsole | CONSOLE_MODE_EXTERNAL | 外部控制台 |
| LaunchInPseudoConsole | CONSOLE_MODE_PSEUDO | 伪控制台 |

### 日志级别 (LogLevel)
| 旧值 | 新值 | 说明 |
|------|------|------|
| VerbosityUnknown | LOG_LEVEL_UNKNOWN | 未知 |
| VerbosityDebug | LOG_LEVEL_DEBUG | 调试 |
| VerbosityError | LOG_LEVEL_ERROR | 错误 |
| VerbosityDisplayText | LOG_LEVEL_INFO | 信息 |

## 消息类型对照表

### 请求消息
| 旧类型 | 新类型 | 说明 |
|--------|--------|------|
| CreateTarget_Req | CreateTargetRequest | 创建目标 |
| Launch_Req | LaunchRequest | 启动进程 |
| Continue_Req | ContinueRequest | 继续执行 |
| StepInto_Req | StepIntoRequest | 单步进入 |
| StepOver_Req | StepOverRequest | 单步跳过 |
| StepOut_Req | StepOutRequest | 单步跳出 |
| AddBreakpoint_Req | AddBreakpointRequest | 添加断点 |
| GetThreads_Req | GetThreadsRequest | 获取线程 |
| GetFrames_Req | GetFramesRequest | 获取栈帧 |

### 响应消息
| 旧类型 | 新类型 | 说明 |
|--------|--------|------|
| CreateTarget_Res | CreateTargetResponse | 创建目标响应 |
| Launch_Res | LaunchResponse | 启动响应 |
| GetThreads_Res | GetThreadsResponse | 获取线程响应 |
| GetFrames_Res | GetFramesResponse | 获取栈帧响应 |
| CommonResponse | ResponseStatus | 通用响应状态 |

### 事件消息
| 旧类型 | 新类型 | 说明 |
|--------|--------|------|
| ProcessInterrupted_Broadcast | ProcessInterruptedEvent | 进程中断事件 |
| ProcessRunning_Broadcast | ProcessRunningEvent | 进程运行事件 |
| ProcessExited_Broadcast | ProcessExitedEvent | 进程退出事件 |
| TargetProcessOutput_Broadcast | ProcessOutputEvent | 进程输出事件 |
| BreakpointAdded_Broadcast | BreakpointAddedEvent | 断点添加事件 |
| LogMessage_Broadcast | LogMessageEvent | 日志消息事件 |

## 最佳实践

1. **使用ProtoConverter工具类**: 不要直接创建proto消息，使用ProtoConverter提供的工厂方法
2. **检查响应状态**: 始终检查ResponseStatus的success字段
3. **处理可选字段**: 使用has_xxx()方法检查可选字段是否存在
4. **批量操作**: 使用CompositeRequest/CompositeResponse进行批量操作以提高效率
5. **错误处理**: 在error_message字段中提供详细的错误信息

## 编译和使用

```bash
# 重新生成proto文件
cmake --build . --target regenerate_protos

# 编译项目
cmake --build .

# 运行示例
./output/CangJieLLDBFrontend 8080
```

## 迁移指南

如果你有使用旧proto定义的代码，请参考以下步骤进行迁移：

1. 更新枚举值名称（全部改为大写下划线格式）
2. 更新消息类型名称（移除下划线，使用统一后缀）
3. 更新字段名称（使用完整单词，避免缩写）
4. 使用ProtoConverter工具类替代手动创建消息
5. 更新响应处理逻辑（CommonResponse → ResponseStatus）

## 参考资料

- [Protocol Buffers官方文档](https://protobuf.dev/)
- [Protocol Buffers风格指南](https://protobuf.dev/programming-guides/style/)
- 项目proto文件: `schema/model.proto`, `schema/protocol.proto`, `schema/protocol_responses.proto`, `schema/broadcasts.proto`
