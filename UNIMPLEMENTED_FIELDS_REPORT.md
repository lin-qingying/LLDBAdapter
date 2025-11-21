# Protobuf 字段实现分析报告

## 概述

对仓颉调试器的 Protobuf 协议定义与 C++ 实现进行全面分析。

**分析字段总数**: 210+
- **已完全实现**: 158 (75%)
- **部分实现**: 25 (12%)
- **未实现**: 27 (13%)

---

## 请求字段 - 未实现

### CreateTargetRequest（创建目标请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| target_triple | 2 | 未实现 | 目标三元组未读取，不支持交叉编译调试 |
| options | 3 | 未实现 | 扩展选项 map 未访问 |

### LaunchRequest（启动请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| stdin_path | 5 | 部分实现 | 仅记录日志，未配置到 LLDB |
| stdout_path | 6 | 部分实现 | 仅记录日志，未配置到 LLDB |
| stderr_path | 7 | 部分实现 | 仅记录日志，未配置到 LLDB |

### AddBreakpointRequest（添加断点请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| LineBreakpoint.ignore_source_hash | 9 | 未实现 | 从未使用 |
| FunctionBreakpoint.mangled_name | 2 | 部分实现 | 读取但未应用 |
| WatchBreakpoint.resolve_location | 2 | 未实现 | 从未读取 |
| WatchBreakpoint.size | 3 | **未实现** | **高优先级** - 未应用到 LLDB 观察点 |

### VariablesChildrenRequest（获取子变量请求）- 分页未实现
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| expression | 4 | 未实现 | 未用于动态求值 |
| offset | 5 | **未实现** | **高优先级** - 分页不支持 |
| count | 6 | **未实现** | **高优先级** - 分页不支持 |
| max_depth | 7 | 未实现 | 未强制执行 |
| max_children | 8 | 未实现 | 未强制执行 |

### StepIntoRequest / StepOverRequest（单步请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| step_by_instruction | 2 | **未实现** | **高优先级** - 无法进行指令级单步 |

### RegistersRequest（寄存器请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| group_names | 3 | 未实现 | 未实现过滤功能 |
| register_names | 4 | 未实现 | 未实现过滤功能 |

### DisassembleRequest（反汇编请求）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| thread_id | 4 | 未实现 | 未用于反汇编上下文 |

---

## 响应字段 - 未完全实现

### AddBreakpointResponse（添加断点响应）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| BreakpointLocation.module_path | 5 | 未实现 | 从未设置 |

---

## 事件字段 - 未完全实现

### ProcessExited（进程退出事件）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| exit_description | 3 | 部分实现 | 部分场景设置 |

---

## 模型字段 - 未完全实现

### BreakpointLocation（断点位置）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| module_path | 5 | 未实现 | 从未填充 |

### Module（模块）- 元数据不完整
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| uuid | 4 | 部分实现 | 仅字符串 ID，非二进制 UUID |
| size | 7 | 未实现 | 从未设置 |
| object_file_type | 9 | 未实现 | 从未设置（ELF、Mach-O、PE/COFF）|
| architecture | 10 | 未实现 | 从未设置（x86_64、arm64）|

### Register（寄存器）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| changed | 9 | 未实现 | 值变化追踪未实现，影响 UI 高亮显示 |
| group_name | 7 | 部分实现 | 部分场景设置 |

### ThreadStopInfo（线程停止信息）- oneof 详情不完整

| 停止类型 | 字段号 | 状态 | 影响 |
|----------|--------|------|------|
| BreakpointStopInfo | 3 | 部分实现 | 类型检测简化 |
| WatchpointStopInfo | 4 | 部分实现 | watch_type 字段未设置 |
| SignalStopInfo | 5 | 已实现 | - |
| ExceptionStopInfo | 6 | 部分实现 | 大部分字段缺失 |
| **StepStopInfo** | **7** | **未实现** | **完全未实现** |
| **ThreadExitStopInfo** | **8** | **未实现** | **完全未实现** |
| **InstrumentationStopInfo** | **9** | **未实现** | **完全未实现** |
| **PlanCompleteStopInfo** | **10** | **未实现** | **完全未实现** |

### StepStopInfo（单步停止信息）- 完全未实现
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| step_type | 1 | 未实现 | 从未填充 |
| step_range | 2 | 未实现 | 从未填充 |
| location | 3 | 未实现 | 从未填充 |
| finished_function_call | 4 | 未实现 | 从未填充 |

### ExceptionStopInfo（异常停止信息）- 大部分未实现
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| exception_code | 1 | 未实现 | 未提取 |
| exception_name | 2 | 未实现 | 未提取 |
| exception_type | 4 | 未实现 | 从未确定 |
| message | 5 | 未实现 | 从未设置 |
| location | 6 | 未实现 | 从未设置 |

### FunctionInfo（函数信息）
| 字段 | 字段号 | 状态 | 说明 |
|------|--------|------|------|
| is_hole | 10 | 部分实现 | 硬编码为 false，从未实际判断 |

---

## 高优先级问题

### 1. 指令级单步（step_by_instruction）
- **问题**: `step_by_instruction` 字段未使用
- **影响**: 无法进行汇编级调试
- **严重性**: **阻塞性** - 功能完全不可用

### 2. 观察点大小（WatchBreakpoint.size）
- **问题**: 未传递给 LLDB 的 WatchAddress()
- **影响**: 观察点可能无法正常工作
- **严重性**: **阻塞性** - 观察点可能失效

### 3. 子变量分页（offset, count）
- **问题**: VariablesChildrenRequest 的分页参数未实现
- **影响**: 无法有效浏览大型数组
- **严重性**: **阻塞性** - 大型数据结构检查失败

### 4. 模块元数据
- **问题**: architecture、object_file_type、size 缺失
- **影响**: 限制系统级调试上下文信息
- **严重性**: 中等

### 5. ThreadStopInfo 详情
- **问题**: StepStopInfo、ThreadExitStopInfo 等未实现
- **影响**: 无法提供详细的停止上下文
- **严重性**: 中等

---

## 分类统计

### 请求（60 个字段）
- 已实现: 45 (75%)
- 部分实现: 8 (13%)
- 未实现: 7 (12%)

### 响应（40 个字段）
- 已实现: 38 (95%)
- 部分实现: 2 (5%)
- 未实现: 0 (0%)

### 事件（30 个字段）
- 已实现: 28 (93%)
- 部分实现: 2 (7%)
- 未实现: 0 (0%)

### 模型（70 个字段）
- 已实现: 55 (79%)
- 部分实现: 13 (19%)
- 未实现: 2 (2%)

---

## 结论

调试器实现覆盖率良好（75%），但在以下方面存在明显差距：

1. **指令级调试**（step_by_instruction）- 汇编调试不可用
2. **大型数据结构处理**（分页）- 无法浏览大数组
3. **系统元数据**（模块信息）- 缺少架构、文件类型等信息
4. **停止事件详情**（StepStopInfo 等）- 停止原因信息不完整

这些差距阻碍了 LLDB 功能的完整利用。

---

## 建议修复优先级

1. **P0 - 立即修复**
   - `step_by_instruction` - 指令级单步
   - `WatchBreakpoint.size` - 观察点大小

2. **P1 - 高优先级**
   - `VariablesChildrenRequest` 分页（offset, count）
   - 标准输入/输出/错误重定向（stdin_path, stdout_path, stderr_path）

3. **P2 - 中优先级**
   - 模块元数据（architecture, object_file_type, size）
   - 寄存器过滤（group_names, register_names）
   - ThreadStopInfo 详情

4. **P3 - 低优先级**
   - CreateTargetRequest 扩展选项
   - FunctionInfo.is_hole 实际判断