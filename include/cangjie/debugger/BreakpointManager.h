// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_DEBUGGER_BREAKPOINT_MANAGER_H
#define CANGJIE_DEBUGGER_BREAKPOINT_MANAGER_H

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "lldb/API/LLDB.h"


// Include shared types from ProtoConverter
#include <request.pb.h>

#include "ProtoConverter.h"

namespace cangjie {
namespace debugger {


/**
 * @brief Represents a single breakpoint with full information
 */
struct BreakpointInfo {
    Cangjie::Debugger::BreakpointType type;
    int64_t lldb_id;
    bool enabled;
    std::string condition;
    uint32_t ignore_count;
    int32_t thread_id;
    int64_t watchpoint_id; // For watchpoints
    std::string error_message;

    // Location information
    std::string file_path;
    int line_number;
    uint64_t address;
    std::string function_name;
    std::string symbol_pattern;
    bool is_regex;

    // Runtime information
    int hit_count;
    bool resolved;

    BreakpointInfo()
        : type(Cangjie::Debugger::BreakpointType::LINE_BREAKPOINT)
        , lldb_id(-1)
        , enabled(true)
        , ignore_count(0)
        , thread_id(0)
        , watchpoint_id(-1)
        , line_number(0)
        , address(0)
        , is_regex(false)
        , hit_count(0)
        , resolved(false) {}
};

/**
 * @brief Breakpoint creation result
 */
struct BreakpointCreateResult {
    bool success;
    BreakpointInfo breakpoint_info;
    std::vector<std::unique_ptr<lldbprotobuf::BreakpointLocation>> locations;

    BreakpointCreateResult() : success(false) {}
};

/**
 * @brief Manages breakpoints for the debugger
 */
class BreakpointManager {
public:
    BreakpointManager();
    ~BreakpointManager();

    // Set LLDB target for breakpoint operations
    void SetTarget(const lldb::SBTarget &target);

    // Get LLDB target
    lldb::SBTarget GetTarget() const;

    // ========================================================================
    // 高级断点管理方法 - 处理 proto 消息
    // ========================================================================

    /**
     * @brief 处理添加断点请求
     */
    BreakpointCreateResult HandleAddBreakpointRequest(
        const lldbprotobuf::AddBreakpointRequest& request);

    /**
     * @brief 处理删除断点请求
     */
    bool HandleRemoveBreakpointRequest(
        const lldbprotobuf::RemoveBreakpointRequest& request,
        std::string& error_message);

    // ========================================================================
    // 具体类型的断点创建方法
    // ========================================================================

    /**
     * @brief 创建行断点
     */
    BreakpointCreateResult CreateLineBreakpoint(
        const std::string& file_path,
        int line_number,
        const std::string& condition = "",
        bool enabled = true,
        uint32_t ignore_count = 0,
        int32_t thread_id = 0);

    /**
     * @brief 创建地址断点
     */
    BreakpointCreateResult CreateAddressBreakpoint(
        uint64_t address,
        const std::string& condition = "",
        bool enabled = true,
        uint32_t ignore_count = 0,
        int32_t thread_id = 0);

    /**
     * @brief 创建函数断点
     */
    BreakpointCreateResult CreateFunctionBreakpoint(
        const std::string& function_name,
        const std::string& condition = "",
        bool enabled = true,
        uint32_t ignore_count = 0,
        int32_t thread_id = 0);

    /**
     * @brief 创建符号断点
     */
    BreakpointCreateResult CreateSymbolBreakpoint(
        const std::string& symbol_pattern,
        bool is_regex = false,
        const std::string& condition = "",
        bool enabled = true,
        uint32_t ignore_count = 0,
        int32_t thread_id = 0);

    /**
     * @brief 创建观察点
     */
    BreakpointCreateResult CreateWatchpoint(
        const std::string& variable_name,
        int32_t thread_id = 0,
        bool read_watch = true,
        bool write_watch = true);

    // ========================================================================
    // 断点操作方法
    // ========================================================================

    /**
     * @brief 根据 ID 删除断点
     */
    bool RemoveBreakpointById(int64_t breakpoint_id, std::string& error_message);

    /**
     * @brief 启用/禁用断点
     */
    bool SetBreakpointEnabled(int64_t breakpoint_id, bool enabled, std::string& error_message);

    /**
     * @brief 设置断点条件
     */
    bool SetBreakpointCondition(int64_t breakpoint_id, const std::string& condition, std::string& error_message);

    /**
     * @brief 设置断点忽略计数
     */
    bool SetBreakpointIgnoreCount(int64_t breakpoint_id, uint32_t ignore_count, std::string& error_message);

    // ========================================================================
    // 断点查询方法
    // ========================================================================

    /**
     * @brief 获取断点信息
     */
    BreakpointInfo* GetBreakpointInfo(int64_t breakpoint_id);

    /**
     * @brief 获取所有断点
     */
    std::vector<BreakpointInfo*> GetAllBreakpoints() const;

    /**
     * @brief 获取指定类型的断点
     */
    std::vector<BreakpointInfo*> GetBreakpointsByType(Cangjie::Debugger::BreakpointType type) const;

    /**
     * @brief 检查断点是否存在
     */
    bool HasBreakpoint(int64_t breakpoint_id) const;

    /**
     * @brief 获取断点数量
     */
    size_t GetBreakpointCount() const;

    // ========================================================================
    // 批量操作方法
    // ========================================================================

    /**
     * @brief 清除所有断点
     */
    void ClearAllBreakpoints(std::string& error_message);

    /**
     * @brief 启用所有断点
     */
    bool EnableAllBreakpoints(std::string& error_message);

    /**
     * @brief 禁用所有断点
     */
    bool DisableAllBreakpoints(std::string& error_message);

    // ========================================================================
    // 兼容性方法 (保持向后兼容)
    // ========================================================================

    bool setBreakpoint(const std::string& file, int line);
    bool removeBreakpoint(const std::string& file, int line);
    bool enableBreakpoint(const std::string& file, int line);
    bool disableBreakpoint(const std::string& file, int line);
    bool setBreakpointCondition(const std::string& file, int line, const std::string& condition);
    bool hasBreakpoint(const std::string& file, int line) const;

private:
    // Helper methods
    BreakpointCreateResult CreateBreakpointResponse(
        const BreakpointInfo& info,
        lldb::SBBreakpoint lldb_bp);

    static lldbprotobuf::SourceLocation* CreateProtoSourceLocation(
        const std::string& file_path,
        int line_number);


    static Cangjie::Debugger::BreakpointType DetectBreakpointType(const lldbprotobuf::AddBreakpointRequest& request);

    // Legacy structures (for backward compatibility)
    using BreakpointKey = std::pair<std::string, int>;
    std::map<BreakpointKey, std::unique_ptr<BreakpointInfo>> legacy_breakpoints_;

    // New structures
    std::map<int64_t, std::unique_ptr<BreakpointInfo>> breakpoints_by_id_;
    lldb::SBTarget target_;
};

} // namespace debugger
} // namespace cangjie

#endif // CANGJIE_DEBUGGER_BREAKPOINT_MANAGER_H