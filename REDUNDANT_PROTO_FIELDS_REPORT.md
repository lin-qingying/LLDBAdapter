# Proto 文件冗余声明分析报告

## 概述

对仓颉调试器 Proto 文件进行分析，识别可能多余、重复或过度设计的字段声明。

**分析文件**：model.proto、request.proto、response.proto、event.proto

**识别问题数**：12 个潜在冗余字段

---

## 澄清：以下字段是必要的，非冗余

| 文件 | 消息 | 字段 | 说明 |
|------|------|------|------|
| request.proto | VariablesRequest | include_recognized_arguments | LLDB API 支持：`SBVariablesOptions::SetIncludeRecognizedArguments()` |
| request.proto | VariablesRequest | include_runtime_support_values | LLDB API 支持：`SBVariablesOptions::SetIncludeRuntimeSupportValues()` |
| request.proto | Request | hash | 协议必需：用于 IDE 匹配请求和响应 |
| response.proto | Response | hash | 协议必需：返回对应请求的 hash |
| model.proto | HashId | hash | 协议必需：封装 hash 值 |

---

## 第一类：LLDB 不支持的功能

这些字段定义了 LLDB API 无法实现或不支持的功能。

| 文件 | 消息 | 字段 | 说明 | 建议 |
|------|------|------|------|------|
| model.proto | Register | children (10) | 寄存器子元素应通过单独请求动态获取，预序列化嵌套增加复杂度 | **考虑删除** |
| request.proto | WatchBreakpoint | resolve_location (2) | 观察点基于地址，无"解析位置到源码"的概念 | **删除** |
| event.proto | Initialized | capabilities (3) | 当前未定义能力位含义，需要明确定义或删除 | **明确定义或删除** |

---

## 第二类：与现有字段重复

这些字段的信息可以从其他已有字段获取。

| 文件 | 消息 | 字段 | 重复来源 | 建议 |
|------|------|------|----------|------|
| model.proto | BreakpointStopInfo | condition (5) | 与 Breakpoint.condition 重复，停止时不需要再返回条件 | **删除** |
| model.proto | FunctionInfo | is_hole (10) | 可从 start_address、end_address、size 关系推断 | **删除** |
| model.proto | ExceptionStopInfo | exception_address (3) | 与 Frame.program_counter 重复 | **标记为可选** |

---

## 第三类：设计过度复杂

### 3.1 多进程相关枚举值

协议目前不支持多进程调试，这些枚举值无法使用。

| 文件 | 枚举 | 值 | 说明 | 建议 |
|------|------|-----|------|------|
| model.proto | StopReason | STOP_REASON_FORK (8) | 协议不支持多进程调试 | **删除** |
| model.proto | StopReason | STOP_REASON_VFORK (9) | 协议不支持多进程调试 | **删除** |
| model.proto | StopReason | STOP_REASON_VFORK_DONE (10) | 协议不支持多进程调试 | **删除** |

### 3.2 响应设计模糊

| 文件 | 消息 | 问题 | 建议 |
|------|------|------|------|
| response.proto | GetFunctionInfoResponse | function (单个) 和 functions (列表) 同时存在，语义冲突 | **只保留 functions** |

### 3.3 UI 层事件枚举值

LLDB 不主动推送这些 UI 层事件，这些枚举值可能无法使用。

| 文件 | 枚举 | 值 | 说明 | 建议 |
|------|------|-----|------|------|
| model.proto | ThreadStateChangeType | SELECTED_FRAME_CHANGED (4) | UI 层事件，LLDB 不推送 | **评估是否需要** |
| model.proto | ThreadStateChangeType | THREAD_SELECTED (5) | UI 层事件，LLDB 不推送 | **评估是否需要** |
| model.proto | BreakpointEventType | COMMAND_CHANGED (8) | LLDB 可能不支持断点命令 | **评估是否需要** |

---

## 第四类：缺失但应添加的字段

| 文件 | 消息 | 缺失字段 | 说明 | 建议 |
|------|------|----------|------|------|
| model.proto | SourceLocation | column | LLDB 支持列号（SBLineEntry::GetColumn），但协议未定义 | **添加** |

---

## 第五类：可简化的复杂结构

### 5.1 ThreadStopInfo oneof 过度设计

当前 ThreadStopInfo 使用 oneof 包含 8 种停止详情，但实际上：
- 4 种从未实现（StepStopInfo、ThreadExitStopInfo、InstrumentationStopInfo、PlanCompleteStopInfo）

**建议**：
- 方案 A：实现这些停止详情
- 方案 B：简化为已实现的几种 + 通用的 description 字符串

### 5.2 源文件哈希功能评估

| 文件 | 消息/字段 | 当前状态 | 建议 |
|------|-----------|----------|------|
| model.proto | Hash 消息 | 定义但未使用 | **评估：IDE 还是调试器负责哈希校验** |
| model.proto | HashAlgorithm 枚举 | 定义但未使用 | **同上** |
| model.proto | SourceLocation.hash | 定义但未使用 | **同上** |
| request.proto | LineBreakpoint.ignore_source_hash | 定义但未使用 | **同上** |

如果决定由 IDE 负责源文件校验，可删除这些定义。如果调试器负责，需要实现。

---

## 优先级建议

### P0 - 立即处理（3 个）
影响协议清晰度：
- `WatchBreakpoint.resolve_location` - 删除
- `BreakpointStopInfo.condition` - 删除
- `GetFunctionInfoResponse` - 统一使用 functions 列表

### P1 - 短期改进（4 个）
- 删除多进程相关 StopReason 枚举值（FORK/VFORK/VFORK_DONE）
- 明确 Initialized.capabilities 的位定义
- 添加 SourceLocation.column

### P2 - 中期评估（4 个）
- 评估 Hash 相关功能的归属（IDE vs 调试器）
- 评估 ThreadStateChangeType UI 事件的必要性
- 简化或完善 ThreadStopInfo 结构
- Register.children 设计评估

### P3 - 长期考虑（1 个）
- FunctionInfo.is_hole 是否需要保留

---

## 总结

| 类别 | 数量 | 主要问题 |
|------|------|----------|
| LLDB 不支持 | 3 | 功能无法实现 |
| 字段重复 | 3 | 信息冗余 |
| 过度设计 | 4 | 复杂但未使用 |
| 需评估 | 6 | 设计决策待定 |
| 应添加 | 1 | 缺少 LLDB 支持的功能 |

**总计**：12 个确定问题 + 6 个待评估

---

## 修正说明

以下字段在之前的分析中被错误标记为冗余，现已更正：

1. **VariablesRequest.include_recognized_arguments** - LLDB API `SetIncludeRecognizedArguments()` 支持，且已实现
2. **VariablesRequest.include_runtime_support_values** - LLDB API `SetIncludeRuntimeSupportValues()` 支持，且已实现
3. **Request/Response.hash** - 协议必需，用于请求-响应匹配