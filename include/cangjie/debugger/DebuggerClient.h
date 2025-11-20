// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_DEBUGGER_DEBUGGER_CLIENT_H
#define CANGJIE_DEBUGGER_DEBUGGER_CLIENT_H

#include "cangjie/debugger/TcpClient.h"

#include "model.pb.h"

#include <string>
#include <vector>
#include <unordered_map>

#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <request.pb.h>

#include "ProtoConverter.h"
#include "BreakpointManager.h"


#ifdef USE_DYNAMIC_LLDB
#include <lldb/API/LLDB.h>
#endif


namespace Cangjie {
namespace Debugger {

class
    DebuggerClient {
public:
    explicit DebuggerClient(TcpClient& tcp_client);
    ~DebuggerClient();

    bool SendInitializedEvent(

        uint64_t capabilities = 0
    ) const;

    bool SendCreateTargetResponse(
        bool success = true,
        const std::string &error_message = "",
        const std::optional<uint64_t> hash = std::nullopt
    ) const;

    bool SendLaunchResponse(
        bool success,
        int64_t process_id,
        const std::string& error_message = "",
        const std::optional<uint64_t> hash = std::nullopt
    ) const;

    bool SendContinueResponse(const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendSuspendResponse(const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendDetachResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendTerminateResponse(const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendExitResponse(const std::optional<uint64_t> hash = std::nullopt) const;

    bool SendStepIntoResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendStepOverResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendStepOutResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendThreadsResponse(bool success = true, const std::vector<lldbprotobuf::Thread>& threads = {}, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendFramesResponse(bool success = true, const std::vector<lldbprotobuf::Frame>& frames = {}, uint32_t total_frames = 0, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendVariablesResponse(bool success = true, const std::vector<lldbprotobuf::Variable>& variables = {}, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;

    bool SendGetValueResponse(bool success, const lldbprotobuf::Value& value, const lldbprotobuf::Variable& variable, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendSetVariableValueResponse(bool success, const lldbprotobuf::Value& value, const lldbprotobuf::Variable& variable, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;

    bool SendVariablesChildrenResponse(bool success, const std::vector<lldbprotobuf::Variable>& children, uint32_t total_children, uint32_t offset, bool has_more, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;

    // Expression Evaluation and Memory Responses
    bool SendEvaluateResponse(bool success, const lldbprotobuf::Variable& variable, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;

    lldbprotobuf::HashId CreateHashId(unsigned long long value) const;

    bool SendReadMemoryResponse(bool success, uint64_t address, const std::string& data, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendWriteMemoryResponse(bool success, uint32_t bytes_written, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;
    bool SendDisassembleResponse(bool success, const std::vector<lldbprotobuf::DisassembleInstruction>& instructions, uint32_t bytes_disassembled, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;



    bool SendAddBreakpointResponse(
        bool success,
        BreakpointType breakpoint_type,
        const lldbprotobuf::Breakpoint& breakpoint,
        const std::vector<lldbprotobuf::BreakpointLocation>& locations,
        const std::string& error_message = "",
        const std::optional<uint64_t> hash = std::nullopt
    ) const;
    bool SendRemoveBreakpointResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;







    // Target and Process Management Responses
    bool SendAttachResponse(bool success = true, const std::string& error_message = "", const std::optional<uint64_t> hash = std::nullopt) const;

    // ========================================================================
    // Event Broadcasting Methods
    // ========================================================================

    /**
     * @brief 发送进程退出事件
     */
    bool SendProcessExitedEvent(int32_t exit_code, const std::string& exit_description = "") const;

    /**
     * @brief 发送进程停止事件
     */
    bool SendProcessStoppedEvent(  lldb::SBThread& thread) const;






    bool ReceiveRequest(lldbprotobuf::Request& request) const;

    /**
     * @brief 处理接收到的CompositeRequest
     * @param request 要处理的请求
     * @return 是否处理成功
     */
    bool HandleRequest(const lldbprotobuf::Request& request);

    /**
     * @brief 运行消息循环，持续接收和处理Request消息
     * @param request_handler 处理Request的回调函数，返回true表示继续循环，false表示退出
     *                         如果为nullptr，则使用默认的HandleRequest方法处理
     */
    void RunMessageLoop(
        const std::function<bool(const lldbprotobuf::Request&)> &request_handler = nullptr
    )  ;

private:
    TcpClient& tcp_client_;
    // std::unique_ptr<IPCManager> io_manager_;

    // 管理器类 - 使用旧命名空间
    mutable std::unique_ptr<cangjie::debugger::BreakpointManager> breakpoint_manager_;


#ifdef USE_DYNAMIC_LLDB
    // LLDB debugger and target
    mutable lldb::SBDebugger debugger_;
    mutable lldb::SBTarget target_;
    mutable lldb::SBProcess process_;
    mutable bool lldb_initialized_;
#endif

    /**
     * @brief 初始化LLDB调试器（如果尚未初始化）
     * @return 是否成功
     */
    bool InitializeLLDB() const;

    // 事件监听线程
    std::thread event_thread_;
    // 事件监听线程运行标志
    std::atomic<bool> event_thread_running_;

    // 专用事件监听器
    mutable lldb::SBListener event_listener_;

    // 变量ID到LLDB SBValue的映射表
    // Key: 变量ID (uint64_t，基于thread_id+frame_index+current_time的哈希)
    // Value: LLDB SBValue对象
    mutable std::unordered_map<uint64_t, lldb::SBValue> variable_id_map_;

    /**
     * @brief 启动事件监听线程
     */
    void StartEventThread();

    /**
     * @brief 停止事件监听线程
     */
    void StopEventThread();

    /**
     * @brief 事件监听线程的主循环函数
     */
    void EventThreadLoop();

    /**
     * @brief 设置所有LLDB事件监听器
     */
    void SetupAllEventListeners()  ;

    /**
     * @brief 清理LLDB资源
     */
    void CleanupLLDB() const;

    // Process termination utilities
    /**
     * @brief 强制终止被调试进程
     * @return 成功返回true
     */
    bool ForceTerminateProcess() const;

    /**
     * @brief 等待进程终止
     * @param timeout_ms 超时时间（毫秒）
     * @return 进程终止返回true
     */
    bool WaitForProcessTermination(int timeout_ms = 5000)  ;

    /**
     * @brief 确保进程已被终止（强制终止）
     */
    void EnsureProcessTerminated()  ;

    // ============================================================================
    // Variable ID Management
    // ============================================================================

    /**
     * @brief 根据变量ID查找LLDB变量
     * @param variable_id 变量ID
     * @return LLDB变量对象，如果未找到则无效
     */
    lldb::SBValue FindVariableById(uint64_t variable_id) const;

    /**
     * @brief 为变量分配ID并存储映射
     * @param thread_id 线程ID
     * @param frame_index 帧索引
     * @param sb_value LLDB变量对象
     * @return 分配的变量ID
     */
    uint64_t AllocateVariableId(uint64_t thread_id, uint32_t frame_index,   lldb::SBValue& sb_value) const;

    /**
     * @brief 从变量映射中清理失效的SBValue对象
     * @return 清理的变量数量
     */
    size_t CleanupInvalidVariables() const;


    // ============================================================================
    // Comprehensive Event Handlers
    // ============================================================================

    /**
     * @brief 处理所有类型的LLDB事件
     */
    void HandleEvent(   lldb::SBEvent& event);

    /**
     * @brief 处理进程相关事件
     */
    void HandleProcessEvent(const lldb::SBEvent& event);

    /**
     * @brief 处理目标相关事件
     */
    void HandleTargetEvent(const lldb::SBEvent& event);

    /**
     * @brief 处理断点相关事件
     */
    void HandleBreakpointEvent(const lldb::SBEvent& event);

    /**
     * @brief 处理线程相关事件
     */
    void HandleThreadEvent(const lldb::SBEvent& event);


    /**
     * @brief 打印断点命中信息
     */
    void LogBreakpointInfo(  lldb::SBThread& thread) const;

    /**
     * @brief 打印已加载模块信息
     */
    static void LogLoadedModules(const lldb::SBEvent& event) ;



    // ============================================================================
    // Request Handlers - Target and Process Management
    // ============================================================================
    bool HandleCreateTargetRequest(const lldbprotobuf::CreateTargetRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleLaunchRequest(const lldbprotobuf::LaunchRequest& req, const std::optional<uint64_t> hash = std::nullopt)  ;
    bool HandleAttachRequest(const lldbprotobuf::AttachRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;

    bool HandleDetachRequest(const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleTerminateRequest(const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleExitRequest(const std::optional<uint64_t> hash = std::nullopt) const;

    // ============================================================================
    // Request Handlers - Execution Control
    // ============================================================================
    bool HandleContinueRequest(const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleSuspendRequest(const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleStepIntoRequest(const lldbprotobuf::StepIntoRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleStepOverRequest(const lldbprotobuf::StepOverRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleStepOutRequest(const lldbprotobuf::StepOutRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;


    // ============================================================================
    // Request Handlers - Breakpoints and Watchpoints
    // ============================================================================
    bool HandleAddBreakpointRequest(const lldbprotobuf::AddBreakpointRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleRemoveBreakpointRequest(const lldbprotobuf::RemoveBreakpointRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;


    // ============================================================================
    // Request Handlers - Expression Evaluation and Variables
    // ============================================================================

    // ============================================================================
    // Request Handlers - Memory and Disassembly
    // ============================================================================

    // ============================================================================
    // Request Handlers - Registers
    // ============================================================================



    // ============================================================================
    // Request Handlers - Console and Commands
    // ============================================================================


    // ============================================================================
    // Request Handlers - Platform and Remote Debugging
    // ============================================================================


    // ============================================================================
    // Request Handlers - Thread Control
    // ============================================================================
    bool HandleThreadsRequest(const lldbprotobuf::ThreadsRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleFramesRequest(const lldbprotobuf::FramesRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleVariablesRequest(const lldbprotobuf::VariablesRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;

    bool HandleGetValueRequest(const lldbprotobuf::GetValueRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleSetVariableValueRequest(const lldbprotobuf::SetVariableValueRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;

    bool HandleVariablesChildrenRequest(const lldbprotobuf::VariablesChildrenRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleEvaluateRequest(const lldbprotobuf::EvaluateRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleReadMemoryRequest(const lldbprotobuf::ReadMemoryRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
  bool HandleWriteMemoryRequest(const lldbprotobuf::WriteMemoryRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;
    bool HandleDisassembleRequest(const lldbprotobuf::DisassembleRequest& req, const std::optional<uint64_t> hash = std::nullopt) const;


    // ============================================================================
    // Request Handlers - Signal Handling
    // ============================================================================


    // ============================================================================
    // Request Handlers - Shell Commands
    // ============================================================================


    // ============================================================================
    // Request Handlers - Symbol Download
    // ============================================================================

};

} // namespace Debugger
} // namespace Cangjie

#endif // CANGJIE_DEBUGGER_DEBUGGER_CLIENT_H
