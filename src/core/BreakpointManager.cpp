/*
 * Copyright 2025 LinQingYing. and contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The use of this source code is governed by the Apache License 2.0,
 * which allows users to freely use, modify, and distribute the code,
 * provided they adhere to the terms of the license.
 *
 * The software is provided "as-is", and the authors are not responsible for
 * any damages or issues arising from its use.
 *
 */

#include "cangjie/debugger/BreakpointManager.h"
#include "cangjie/debugger/Logger.h"
#include "cangjie/debugger/ProtoConverter.h"

namespace cangjie {
namespace debugger {

// Type alias to resolve namespace conflicts
using BreakpointType = Cangjie::Debugger::BreakpointType;

BreakpointManager::BreakpointManager() {
    LOG_INFO("BreakpointManager created");
}

BreakpointManager::~BreakpointManager() {
    std::string error_message;
    ClearAllBreakpoints(error_message);
    LOG_INFO("BreakpointManager destroyed");
}

void BreakpointManager::SetTarget(const lldb::SBTarget &target) {
    target_ = target;
    LOG_INFO("BreakpointManager: Set LLDB target");
}

lldb::SBTarget BreakpointManager::GetTarget() const {
    return target_;
}

// ========================================================================
// 高级断点管理方法 - 处理 proto 消息
// ========================================================================

BreakpointCreateResult BreakpointManager::HandleAddBreakpointRequest(
    const lldbprotobuf::AddBreakpointRequest& request) {

    BreakpointCreateResult result;

    if (!target_.IsValid()) {
        result.breakpoint_info.error_message = "No valid target available";
        LOG_ERROR("BreakpointManager: No valid target available");
        return result;
    }

    // 检测断点类型
    BreakpointType type = DetectBreakpointType(request);
    result.breakpoint_info.type = type;

    // 根据请求类型创建相应的断点
    switch (type) {
        case BreakpointType::LINE_BREAKPOINT: {
            if (!request.has_line()) {
                result.breakpoint_info.error_message = "Line breakpoint request missing line information";
                break;
            }

            const auto& line_bp = request.line();
            result = CreateLineBreakpoint(
                line_bp.file(),
                line_bp.line(),
                request.condition(),
                request.enabled(),
                request.ignore_count(),
                request.thread_id().id());
            break;
        }

        case BreakpointType::ADDRESS_BREAKPOINT: {
            if (!request.has_address()) {
                result.breakpoint_info.error_message = "Address breakpoint request missing address information";
                break;
            }

            const auto& addr_bp = request.address();
            result = CreateAddressBreakpoint(
                addr_bp.address(),
                request.condition(),
                request.enabled(),
                request.ignore_count(),
                request.thread_id().id());
            break;
        }

        case BreakpointType::FUNCTION_BREAKPOINT: {
            if (!request.has_function()) {
                result.breakpoint_info.error_message = "Function breakpoint request missing function information";
                break;
            }

            const auto& func_bp = request.function();
            result = CreateFunctionBreakpoint(
                func_bp.function_name(),
                request.condition(),
                request.enabled(),
                request.ignore_count(),
                request.thread_id().id());
            break;
        }

        case BreakpointType::SYMBOL_BREAKPOINT: {
            if (!request.has_symbol()) {
                result.breakpoint_info.error_message = "Symbol breakpoint request missing symbol information";
                break;
            }

            const auto& symbol_bp = request.symbol();
            result = CreateSymbolBreakpoint(
                symbol_bp.pattern(),
                symbol_bp.is_regex(),
                request.condition(),
                request.enabled(),
                request.ignore_count(),
                request.thread_id().id());
            break;
        }

        case BreakpointType::WATCH_BREAKPOINT: {
            if (!request.has_watchpoint()) {
                result.breakpoint_info.error_message = "Watchpoint request missing watchpoint information";
                break;
            }

            const auto& watchpoint_bp = request.watchpoint();
            result = CreateWatchpoint(
                std::to_string(watchpoint_bp.value_id().id()),  // 使用 value_id 作为变量名
                request.thread_id().id(),
                watchpoint_bp.watch_read(),
                watchpoint_bp.watch_write()
            );
            break;
        }


    }

    if (result.success && result.breakpoint_info.lldb_id > 0) {
        // 保存断点信息
        breakpoints_by_id_[result.breakpoint_info.lldb_id] =
            std::make_unique<BreakpointInfo>(result.breakpoint_info);

        LOG_INFO("BreakpointManager: Created breakpoint with ID " +
                 std::to_string(result.breakpoint_info.lldb_id));
    } else {
        LOG_ERROR("BreakpointManager: Failed to create breakpoint: " +
                  result.breakpoint_info.error_message);
    }

    return result;
}

bool BreakpointManager::HandleRemoveBreakpointRequest(
    const lldbprotobuf::RemoveBreakpointRequest& request,
    std::string& error_message) {

    if (!request.has_breakpoint_id()) {
        error_message = "RemoveBreakpointRequest missing breakpoint ID";
        return false;
    }

    return RemoveBreakpointById(request.breakpoint_id().id(), error_message);
}

// ========================================================================
// 具体类型的断点创建方法
// ========================================================================

BreakpointCreateResult BreakpointManager::CreateLineBreakpoint(
    const std::string& file_path,
    int line_number,
    const std::string& condition,
    bool enabled,
    uint32_t ignore_count,
    int32_t thread_id) {

    BreakpointCreateResult result;
    result.breakpoint_info.type = BreakpointType::LINE_BREAKPOINT;
    result.breakpoint_info.file_path = file_path;
    result.breakpoint_info.line_number = line_number;
    result.breakpoint_info.condition = condition;
    result.breakpoint_info.enabled = enabled;
    result.breakpoint_info.ignore_count = ignore_count;
    result.breakpoint_info.thread_id = thread_id;

    // 创建 LLDB 行断点
    lldb::SBFileSpec file_spec(file_path.c_str());
    lldb::SBBreakpoint lldb_bp = target_.BreakpointCreateByLocation(file_spec, line_number);

    if (!lldb_bp.IsValid()) {
        result.breakpoint_info.error_message = "Failed to create LLDB line breakpoint";
        return result;
    }

    // 设置断点属性
    if (!condition.empty()) {
        lldb_bp.SetCondition(condition.c_str());
    }

    lldb_bp.SetEnabled(enabled);
    lldb_bp.SetIgnoreCount(ignore_count);

    if (thread_id > 0) {
        lldb_bp.SetThreadID(thread_id);
    }

    // 获取断点位置信息
    size_t num_locations = lldb_bp.GetNumLocations();
    for (size_t i = 0; i < num_locations; ++i) {
        lldb::SBBreakpointLocation lldb_loc = lldb_bp.GetLocationAtIndex(i);
        if (lldb_loc.IsValid()) {
            lldb::SBAddress addr = lldb_loc.GetAddress();
            if (addr.IsValid()) {
                // 创建源码位置信息
                lldbprotobuf::SourceLocation location_info;
                lldb::SBLineEntry line_entry = addr.GetLineEntry();
                if (line_entry.IsValid()) {
                    lldb::SBFileSpec location_file_spec = line_entry.GetFileSpec();
                    if (location_file_spec.IsValid()) {
                        char file_path_buffer[1024];
                        location_file_spec.GetPath(file_path_buffer, sizeof(file_path_buffer));
                        location_info = Cangjie::Debugger::ProtoConverter::CreateSourceLocation(
                            file_path_buffer,
                            line_entry.GetLine()
                        );
                    }
                }

                // 使用 ProtoConverter 创建 BreakpointLocation
                auto proto_location = std::make_unique<lldbprotobuf::BreakpointLocation>(
                    Cangjie::Debugger::ProtoConverter::CreateBreakpointLocation(
                        lldb_loc.GetID(),
                        addr.GetLoadAddress(target_),
                        lldb_loc.IsResolved(),
                        location_info
                    )
                );
                result.locations.push_back(std::move(proto_location));
            }
        }
    }

    result.breakpoint_info.lldb_id = lldb_bp.GetID();
    result.breakpoint_info.resolved = num_locations > 0;
    result.success = true;

    LOG_INFO("Created line breakpoint at " + file_path + ":" + std::to_string(line_number) +
             " (ID: " + std::to_string(result.breakpoint_info.lldb_id) + ")");

    return result;
}

BreakpointCreateResult BreakpointManager::CreateAddressBreakpoint(
    uint64_t address,
    const std::string& condition,
    bool enabled,
    uint32_t ignore_count,
    int32_t thread_id) {

    BreakpointCreateResult result;
    result.breakpoint_info.type = BreakpointType::ADDRESS_BREAKPOINT;
    result.breakpoint_info.address = address;
    result.breakpoint_info.condition = condition;
    result.breakpoint_info.enabled = enabled;
    result.breakpoint_info.ignore_count = ignore_count;
    result.breakpoint_info.thread_id = thread_id;

    // 创建 LLDB 地址断点
    lldb::SBBreakpoint lldb_bp = target_.BreakpointCreateByAddress(address);

    if (!lldb_bp.IsValid()) {
        result.breakpoint_info.error_message = "Failed to create LLDB address breakpoint";
        return result;
    }

    // 设置断点属性
    if (!condition.empty()) {
        lldb_bp.SetCondition(condition.c_str());
    }

    lldb_bp.SetEnabled(enabled);
    lldb_bp.SetIgnoreCount(ignore_count);

    if (thread_id > 0) {
        lldb_bp.SetThreadID(thread_id);
    }

    result.breakpoint_info.lldb_id = lldb_bp.GetID();
    result.breakpoint_info.resolved = true;
    result.success = true;

    LOG_INFO("Created address breakpoint at 0x" + std::to_string(address) +
             " (ID: " + std::to_string(result.breakpoint_info.lldb_id) + ")");

    return result;
}

BreakpointCreateResult BreakpointManager::CreateFunctionBreakpoint(
    const std::string& function_name,
    const std::string& condition,
    bool enabled,
    uint32_t ignore_count,
    int32_t thread_id) {

    BreakpointCreateResult result;
    result.breakpoint_info.type = BreakpointType::FUNCTION_BREAKPOINT;
    result.breakpoint_info.function_name = function_name;
    result.breakpoint_info.condition = condition;
    result.breakpoint_info.enabled = enabled;
    result.breakpoint_info.ignore_count = ignore_count;
    result.breakpoint_info.thread_id = thread_id;

    // 创建 LLDB 函数断点
    lldb::SBBreakpoint lldb_bp = target_.BreakpointCreateByName(function_name.c_str());

    if (!lldb_bp.IsValid()) {
        result.breakpoint_info.error_message = "Failed to create LLDB function breakpoint";
        return result;
    }

    // 设置断点属性
    if (!condition.empty()) {
        lldb_bp.SetCondition(condition.c_str());
    }

    lldb_bp.SetEnabled(enabled);
    lldb_bp.SetIgnoreCount(ignore_count);

    if (thread_id > 0) {
        lldb_bp.SetThreadID(thread_id);
    }

    // 获取断点位置信息
    size_t num_locations = lldb_bp.GetNumLocations();
    for (size_t i = 0; i < num_locations; ++i) {
        lldb::SBBreakpointLocation lldb_loc = lldb_bp.GetLocationAtIndex(i);
        if (lldb_loc.IsValid()) {
            lldb::SBAddress addr = lldb_loc.GetAddress();
            if (addr.IsValid()) {
                // 创建源码位置信息
                lldbprotobuf::SourceLocation location_info;
                lldb::SBLineEntry line_entry = addr.GetLineEntry();
                if (line_entry.IsValid()) {
                    lldb::SBFileSpec location_file_spec = line_entry.GetFileSpec();
                    if (location_file_spec.IsValid()) {
                        char file_path_buffer[1024];
                        location_file_spec.GetPath(file_path_buffer, sizeof(file_path_buffer));
                        location_info = Cangjie::Debugger::ProtoConverter::CreateSourceLocation(
                            file_path_buffer,
                            line_entry.GetLine()
                        );
                    }
                }

                // 使用 ProtoConverter 创建 BreakpointLocation
                auto proto_location = std::make_unique<lldbprotobuf::BreakpointLocation>(
                    Cangjie::Debugger::ProtoConverter::CreateBreakpointLocation(
                        lldb_loc.GetID(),
                        addr.GetLoadAddress(target_),
                        lldb_loc.IsResolved(),
                        location_info
                    )
                );
                result.locations.push_back(std::move(proto_location));
            }
        }
    }

    result.breakpoint_info.lldb_id = lldb_bp.GetID();
    result.breakpoint_info.resolved = num_locations > 0;
    result.success = true;

    LOG_INFO("Created function breakpoint for " + function_name +
             " (ID: " + std::to_string(result.breakpoint_info.lldb_id) + ")");

    return result;
}

BreakpointCreateResult BreakpointManager::CreateSymbolBreakpoint(
    const std::string& symbol_pattern,
    bool is_regex,
    const std::string& condition,
    bool enabled,
    uint32_t ignore_count,
    int32_t thread_id) {

    BreakpointCreateResult result;
    result.breakpoint_info.type = BreakpointType::SYMBOL_BREAKPOINT;
    result.breakpoint_info.symbol_pattern = symbol_pattern;
    result.breakpoint_info.is_regex = is_regex;
    result.breakpoint_info.condition = condition;
    result.breakpoint_info.enabled = enabled;
    result.breakpoint_info.ignore_count = ignore_count;
    result.breakpoint_info.thread_id = thread_id;

    // 创建 LLDB 符号断点
    lldb::SBBreakpoint lldb_bp;
    if (is_regex) {
        lldb_bp = target_.BreakpointCreateByRegex(symbol_pattern.c_str());
    } else {
        lldb_bp = target_.BreakpointCreateByName(symbol_pattern.c_str());
    }

    if (!lldb_bp.IsValid()) {
        result.breakpoint_info.error_message = "Failed to create LLDB symbol breakpoint";
        return result;
    }

    // 设置断点属性
    if (!condition.empty()) {
        lldb_bp.SetCondition(condition.c_str());
    }

    lldb_bp.SetEnabled(enabled);
    lldb_bp.SetIgnoreCount(ignore_count);

    if (thread_id > 0) {
        lldb_bp.SetThreadID(thread_id);
    }

    // 获取断点位置信息
    size_t num_locations = lldb_bp.GetNumLocations();
    for (size_t i = 0; i < num_locations; ++i) {
        lldb::SBBreakpointLocation lldb_loc = lldb_bp.GetLocationAtIndex(i);
        if (lldb_loc.IsValid()) {
            lldb::SBAddress addr = lldb_loc.GetAddress();
            if (addr.IsValid()) {
                // 创建源码位置信息
                lldbprotobuf::SourceLocation location_info;
                lldb::SBLineEntry line_entry = addr.GetLineEntry();
                if (line_entry.IsValid()) {
                    lldb::SBFileSpec location_file_spec = line_entry.GetFileSpec();
                    if (location_file_spec.IsValid()) {
                        char file_path_buffer[1024];
                        location_file_spec.GetPath(file_path_buffer, sizeof(file_path_buffer));
                        location_info = Cangjie::Debugger::ProtoConverter::CreateSourceLocation(
                            file_path_buffer,
                            line_entry.GetLine()
                        );
                    }
                }

                // 使用 ProtoConverter 创建 BreakpointLocation
                auto proto_location = std::make_unique<lldbprotobuf::BreakpointLocation>(
                    Cangjie::Debugger::ProtoConverter::CreateBreakpointLocation(
                        lldb_loc.GetID(),
                        addr.GetLoadAddress(target_),
                        lldb_loc.IsResolved(),
                        location_info
                    )
                );
                result.locations.push_back(std::move(proto_location));
            }
        }
    }

    result.breakpoint_info.lldb_id = lldb_bp.GetID();
    result.breakpoint_info.resolved = num_locations > 0;
    result.success = true;

    LOG_INFO("Created symbol breakpoint for pattern " + symbol_pattern +
             " (ID: " + std::to_string(result.breakpoint_info.lldb_id) + ")");

    return result;
}

BreakpointCreateResult BreakpointManager::CreateWatchpoint(
    const std::string& variable_name,
    int32_t thread_id,
    bool read_watch,
    bool write_watch) {

    BreakpointCreateResult result;
    result.breakpoint_info.type = BreakpointType::WATCH_BREAKPOINT;
    result.breakpoint_info.thread_id = thread_id;

    // 注意：观察点的实现比较复杂，需要获取当前进程和线程
    // 这里提供基础框架，实际实现需要更多上下文信息

    lldb::SBProcess process = target_.GetProcess();
    if (!process.IsValid()) {
        result.breakpoint_info.error_message = "No valid process for watchpoint creation";
        return result;
    }

    lldb::SBThread thread = (thread_id > 0) ?
        process.GetThreadByID(thread_id) : process.GetSelectedThread();

    if (!thread.IsValid()) {
        result.breakpoint_info.error_message = "No valid thread for watchpoint creation";
        return result;
    }

    lldb::SBFrame frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) {
        result.breakpoint_info.error_message = "No valid frame for watchpoint creation";
        return result;
    }

    lldb::SBValue variable = frame.FindVariable(variable_name.c_str());
    if (!variable.IsValid()) {
        result.breakpoint_info.error_message = "Variable not found: " + variable_name;
        return result;
    }

    lldb::SBError error;
    lldb::SBWatchpoint watchpoint = variable.Watch(true, read_watch, write_watch, error);

    if (!watchpoint.IsValid() || error.Fail()) {
        result.breakpoint_info.error_message = "Failed to create watchpoint: " +
            std::string(error.GetCString() ? error.GetCString() : "Unknown error");
        return result;
    }

    result.breakpoint_info.lldb_id = watchpoint.GetID();
    result.breakpoint_info.resolved = true;
    result.success = true;

    LOG_INFO("Created watchpoint for variable " + variable_name +
             " (ID: " + std::to_string(result.breakpoint_info.lldb_id) + ")");

    return result;
}

// ========================================================================
// 断点操作方法
// ========================================================================

bool BreakpointManager::RemoveBreakpointById(int64_t breakpoint_id, std::string& error_message) {
    auto it = breakpoints_by_id_.find(breakpoint_id);
    if (it == breakpoints_by_id_.end()) {
        error_message = "Breakpoint with ID " + std::to_string(breakpoint_id) + " not found";
        return false;
    }

    bool removed = false;

    // 根据断点类型选择正确的删除方法
    if (it->second->type == BreakpointType::WATCH_BREAKPOINT) {
        // 观察点使用 DeleteWatchpoint
        removed = target_.DeleteWatchpoint(breakpoint_id);
    } else {
        // 其他断点类型使用 BreakpointDelete
        removed = target_.BreakpointDelete(breakpoint_id);
    }

    if (!removed) {
        error_message = "Failed to delete breakpoint/watchpoint from LLDB";
        return false;
    }

    // 从管理器中移除
    breakpoints_by_id_.erase(it);

    LOG_INFO("Removed breakpoint/watchpoint with ID " + std::to_string(breakpoint_id));
    return true;
}

bool BreakpointManager::SetBreakpointEnabled(int64_t breakpoint_id, bool enabled, std::string& error_message) {
    auto it = breakpoints_by_id_.find(breakpoint_id);
    if (it == breakpoints_by_id_.end()) {
        error_message = "Breakpoint with ID " + std::to_string(breakpoint_id) + " not found";
        return false;
    }

    lldb::SBBreakpoint lldb_bp = target_.FindBreakpointByID(breakpoint_id);
    if (!lldb_bp.IsValid()) {
        error_message = "LLDB breakpoint not found";
        return false;
    }

    lldb_bp.SetEnabled(enabled);
    it->second->enabled = enabled;

    LOG_INFO("Set breakpoint " + std::to_string(breakpoint_id) + " enabled=" +
             std::string(enabled ? "true" : "false"));
    return true;
}

bool BreakpointManager::SetBreakpointCondition(int64_t breakpoint_id, const std::string& condition, std::string& error_message) {
    auto it = breakpoints_by_id_.find(breakpoint_id);
    if (it == breakpoints_by_id_.end()) {
        error_message = "Breakpoint with ID " + std::to_string(breakpoint_id) + " not found";
        return false;
    }

    lldb::SBBreakpoint lldb_bp = target_.FindBreakpointByID(breakpoint_id);
    if (!lldb_bp.IsValid()) {
        error_message = "LLDB breakpoint not found";
        return false;
    }

    lldb_bp.SetCondition(condition.c_str());
    it->second->condition = condition;

    LOG_INFO("Set condition for breakpoint " + std::to_string(breakpoint_id) + ": " + condition);
    return true;
}

bool BreakpointManager::SetBreakpointIgnoreCount(int64_t breakpoint_id, uint32_t ignore_count, std::string& error_message) {
    auto it = breakpoints_by_id_.find(breakpoint_id);
    if (it == breakpoints_by_id_.end()) {
        error_message = "Breakpoint with ID " + std::to_string(breakpoint_id) + " not found";
        return false;
    }

    lldb::SBBreakpoint lldb_bp = target_.FindBreakpointByID(breakpoint_id);
    if (!lldb_bp.IsValid()) {
        error_message = "LLDB breakpoint not found";
        return false;
    }

    lldb_bp.SetIgnoreCount(ignore_count);
    it->second->ignore_count = ignore_count;

    LOG_INFO("Set ignore count for breakpoint " + std::to_string(breakpoint_id) + ": " +
             std::to_string(ignore_count));
    return true;
}

// ========================================================================
// 断点查询方法
// ========================================================================

BreakpointInfo* BreakpointManager::GetBreakpointInfo(int64_t breakpoint_id) {
    auto it = breakpoints_by_id_.find(breakpoint_id);
    return (it != breakpoints_by_id_.end()) ? it->second.get() : nullptr;
}

std::vector<BreakpointInfo*> BreakpointManager::GetAllBreakpoints() const {
    std::vector<BreakpointInfo*> result;
    for (const auto& pair : breakpoints_by_id_) {
        result.push_back(pair.second.get());
    }
    return result;
}

std::vector<BreakpointInfo*> BreakpointManager::GetBreakpointsByType(BreakpointType type) const {
    std::vector<BreakpointInfo*> result;
    for (const auto& pair : breakpoints_by_id_) {
        if (pair.second->type == type) {
            result.push_back(pair.second.get());
        }
    }
    return result;
}

bool BreakpointManager::HasBreakpoint(int64_t breakpoint_id) const {
    return breakpoints_by_id_.find(breakpoint_id) != breakpoints_by_id_.end();
}

size_t BreakpointManager::GetBreakpointCount() const {
    return breakpoints_by_id_.size();
}

// ========================================================================
// 批量操作方法
// ========================================================================

void BreakpointManager::ClearAllBreakpoints(std::string& error_message) {
    // 从 LLDB 删除所有断点和观察点
    bool has_error = false;
    for (const auto& pair : breakpoints_by_id_) {
        bool removed = false;
        if (pair.second->type == BreakpointType::WATCH_BREAKPOINT) {
            removed = target_.DeleteWatchpoint(pair.first);
        } else {
            removed = target_.BreakpointDelete(pair.first);
        }

        if (!removed) {
            has_error = true;
            error_message += "Failed to delete breakpoint/watchpoint " + std::to_string(pair.first) + "; ";
        }
    }

    // 清空管理器中的断点
    breakpoints_by_id_.clear();
    legacy_breakpoints_.clear();

    if (!has_error) {
        error_message.clear(); // 如果没有错误，清空错误消息
    }

    LOG_INFO("Cleared all breakpoints and watchpoints");
}

bool BreakpointManager::EnableAllBreakpoints(std::string& error_message) {
    bool all_success = true;
    for (auto& pair : breakpoints_by_id_) {
        lldb::SBBreakpoint lldb_bp = target_.FindBreakpointByID(pair.first);
        if (lldb_bp.IsValid()) {
            lldb_bp.SetEnabled(true);
            pair.second->enabled = true;
        } else {
            all_success = false;
            error_message += "Failed to enable breakpoint " + std::to_string(pair.first) + "; ";
        }
    }

    LOG_INFO("Enabled all breakpoints");
    return all_success;
}

bool BreakpointManager::DisableAllBreakpoints(std::string& error_message) {
    bool all_success = true;
    for (auto& pair : breakpoints_by_id_) {
        lldb::SBBreakpoint lldb_bp = target_.FindBreakpointByID(pair.first);
        if (lldb_bp.IsValid()) {
            lldb_bp.SetEnabled(false);
            pair.second->enabled = false;
        } else {
            all_success = false;
            error_message += "Failed to disable breakpoint " + std::to_string(pair.first) + "; ";
        }
    }

    LOG_INFO("Disabled all breakpoints");
    return all_success;
}

// ========================================================================
// 兼容性方法 (保持向后兼容)
// ========================================================================

bool BreakpointManager::setBreakpoint(const std::string& file, int line) {
    BreakpointCreateResult result = CreateLineBreakpoint(file, line);
    return result.success;
}

bool BreakpointManager::removeBreakpoint(const std::string& file, int line) {
    // 首先尝试从新的管理器中查找
    for (const auto& pair : breakpoints_by_id_) {
        const auto& info = pair.second;
        if (info->type == BreakpointType::LINE_BREAKPOINT &&
            info->file_path == file &&
            info->line_number == line) {

            std::string error_message;
            return RemoveBreakpointById(pair.first, error_message);
        }
    }

    // 检查 legacy breakpoints
    BreakpointKey key = std::make_pair(file, line);
    auto it = legacy_breakpoints_.find(key);
    if (it != legacy_breakpoints_.end()) {
        legacy_breakpoints_.erase(it);
        return true;
    }

    return false;
}

bool BreakpointManager::enableBreakpoint(const std::string& file, int line) {
    for (const auto& pair : breakpoints_by_id_) {
        const auto& info = pair.second;
        if (info->type == BreakpointType::LINE_BREAKPOINT &&
            info->file_path == file &&
            info->line_number == line) {

            std::string error_message;
            return SetBreakpointEnabled(pair.first, true, error_message);
        }
    }
    return false;
}

bool BreakpointManager::disableBreakpoint(const std::string& file, int line) {
    for (const auto& pair : breakpoints_by_id_) {
        const auto& info = pair.second;
        if (info->type == BreakpointType::LINE_BREAKPOINT &&
            info->file_path == file &&
            info->line_number == line) {

            std::string error_message;
            return SetBreakpointEnabled(pair.first, false, error_message);
        }
    }
    return false;
}

bool BreakpointManager::setBreakpointCondition(const std::string& file, int line, const std::string& condition) {
    for (const auto& pair : breakpoints_by_id_) {
        const auto& info = pair.second;
        if (info->type == BreakpointType::LINE_BREAKPOINT &&
            info->file_path == file &&
            info->line_number == line) {

            std::string error_message;
            return SetBreakpointCondition(pair.first, condition, error_message);
        }
    }
    return false;
}

bool BreakpointManager::hasBreakpoint(const std::string& file, int line) const {
    for (const auto& pair : breakpoints_by_id_) {
        const auto& info = pair.second;
        if (info->type == BreakpointType::LINE_BREAKPOINT &&
            info->file_path == file &&
            info->line_number == line) {
            return true;
        }
    }

    // 检查 legacy breakpoints
    BreakpointKey key = std::make_pair(file, line);
    return legacy_breakpoints_.find(key) != legacy_breakpoints_.end();
}

// ========================================================================
// Helper methods
// ========================================================================

BreakpointType BreakpointManager::DetectBreakpointType(const lldbprotobuf::AddBreakpointRequest& request) {
    if (request.has_line()) return BreakpointType::LINE_BREAKPOINT;
    if (request.has_address()) return BreakpointType::ADDRESS_BREAKPOINT;
    if (request.has_function()) return BreakpointType::FUNCTION_BREAKPOINT;
    if (request.has_symbol()) return BreakpointType::SYMBOL_BREAKPOINT;
    if (request.has_watchpoint()) return BreakpointType::WATCH_BREAKPOINT;
    return BreakpointType::LINE_BREAKPOINT; // 默认值
}

lldbprotobuf::SourceLocation* BreakpointManager::CreateProtoSourceLocation(
    const std::string& file_path, int line_number) {

    auto location = std::make_unique<lldbprotobuf::SourceLocation>();
    if (!file_path.empty()) {
        location->set_file_path(file_path);
    }
    if (line_number > 0) {
        location->set_line(line_number);
    }
    return location.release();
}


} // namespace debugger
} // namespace cangjie