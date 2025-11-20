// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/debugger/DebuggerClient.h"
#include "cangjie/debugger/ProtoConverter.h"
#include "cangjie/debugger/Logger.h"

#include <functional>
#include <memory>
#include <csignal>
#include <vector>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif


namespace Cangjie::Debugger {
    DebuggerClient::DebuggerClient(TcpClient &tcp_client)
        : tcp_client_(tcp_client)
          , breakpoint_manager_(std::make_unique<cangjie::debugger::BreakpointManager>())
          , debugger_()
          , target_()
          , process_()
          , lldb_initialized_(false)
          , event_thread_()
          , event_thread_running_(false)
          , event_listener_()
          , variable_id_map_() {
        // 在构造时初始化 LLDB
        InitializeLLDB();
    }

    DebuggerClient::~DebuggerClient() {
        LOG_INFO("DebuggerClient destructor called");

        // 确保进程被强制终止
        EnsureProcessTerminated();

        // 停止事件监听线程
        StopEventThread();

        // 清理LLDB资源
        CleanupLLDB();

        LOG_INFO("DebuggerClient destructor completed");
    }

    bool DebuggerClient::SendInitializedEvent(

        uint64_t capabilities) const {
        auto initialized_event = ProtoConverter::CreateInitializedEvent(

            capabilities
        );

        lldbprotobuf::Event event;
        *event.mutable_initialized() = initialized_event;


        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendCreateTargetResponse(
        bool success,
        const std::string &error_message,
        const std::optional<uint64_t> hash) const {
        auto create_target_resp = ProtoConverter::CreateCreateTargetResponse(
            success,
            error_message
        );

        lldbprotobuf::Response response;

        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_create_target() = create_target_resp;

        LOG_INFO("Sending CreateTarget response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendLaunchResponse(
        bool success,
        int64_t process_id,
        const std::string &error_message,
        const std::optional<uint64_t> hash) const {
        auto launch_resp = ProtoConverter::CreateLaunchResponse(
            success,
            process_id,
            error_message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_launch() = launch_resp;

        LOG_INFO("Sending Launch response: success=" + std::to_string(success) +
            ", process_id=" + std::to_string(process_id));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendContinueResponse(const std::optional<uint64_t> hash) const {
        auto continue_resp = ProtoConverter::CreateContinueResponse(
            true, // success
            "" // no error message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_continue_() = continue_resp;

        LOG_INFO("Sending Continue response");
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendSuspendResponse(const std::optional<uint64_t> hash) const {
        auto suspend_resp = ProtoConverter::CreateSuspendResponse(
            true, // success
            "" // no error message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_suspend() = suspend_resp;

        LOG_INFO("Sending Suspend response");
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendDetachResponse(bool success, const std::string &error_message,
                                            const std::optional<uint64_t> hash) const {
        auto detach_resp = ProtoConverter::CreateDetachResponse(success, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_detach() = detach_resp;

        LOG_INFO("Sending Detach response");
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendTerminateResponse(const std::optional<uint64_t> hash) const {
        auto kill_resp = ProtoConverter::CreateTerminateResponse(true, "");

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_kill() = kill_resp;

        LOG_INFO("Sending Kill response");
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendExitResponse(const std::optional<uint64_t> hash) const {
        auto exit_resp = ProtoConverter::CreateExitResponse(true, "");

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_exit() = exit_resp;

        LOG_INFO("Sending Exit response");
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendStepIntoResponse(bool success, const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto step_into_resp = ProtoConverter::CreateStepIntoResponse(success, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_step_into() = step_into_resp;

        LOG_INFO("Sending StepInto response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendStepOverResponse(bool success, const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto step_over_resp = ProtoConverter::CreateStepOverResponse(success, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_step_over() = step_over_resp;

        LOG_INFO("Sending StepOver response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendStepOutResponse(bool success, const std::string &error_message,
                                             const std::optional<uint64_t> hash) const {
        auto step_out_resp = ProtoConverter::CreateStepOutResponse(success, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_step_out() = step_out_resp;

        LOG_INFO("Sending StepOut response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendAttachResponse(bool success, const std::string &error_message,
                                            const std::optional<uint64_t> hash) const {
        auto attach_resp = ProtoConverter::CreateAttachResponse(success, -1, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_attach() = attach_resp;

        LOG_INFO("Sending Attach response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendThreadsResponse(bool success, const std::vector<lldbprotobuf::Thread> &threads,
                                             const std::string &error_message,
                                             const std::optional<uint64_t> hash) const {
        auto threads_resp = ProtoConverter::CreateThreadsResponse(success, threads, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_threads() = threads_resp;

        LOG_INFO(
            "Sending Threads response: success=" + std::to_string(success) + ", thread_count=" + std::to_string(threads.
                size()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendFramesResponse(bool success, const std::vector<lldbprotobuf::Frame> &frames,
                                            uint32_t total_frames, const std::string &error_message,
                                            const std::optional<uint64_t> hash) const {
        auto frames_resp = ProtoConverter::CreateFramesResponse(success, frames, total_frames, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_frames() = frames_resp;

        LOG_INFO(
            "Sending Frames response: success=" + std::to_string(success) + ", frame_count=" + std::to_string(frames.
                size()) + ", total_frames=" + std::to_string(total_frames));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendVariablesResponse(bool success, const std::vector<lldbprotobuf::Variable> &variables,
                                               const std::string &error_message,
                                               const std::optional<uint64_t> hash) const {
        auto variables_resp = ProtoConverter::CreateVariablesResponse(success, variables, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_variables() = variables_resp;

        LOG_INFO(
            "Sending Variables response: success=" + std::to_string(success) + ", variable_count=" + std::to_string(
                variables.size()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendGetValueResponse(bool success, const lldbprotobuf::Value &value,
                                              const lldbprotobuf::Variable &variable,
                                              const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto get_value_resp = ProtoConverter::CreateGetValueResponse(success, value, variable, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_get_value() = get_value_resp;

        LOG_INFO("Sending GetValue response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendSetVariableValueResponse(bool success, const lldbprotobuf::Value &value,
                                                      const lldbprotobuf::Variable &variable,
                                                      const std::string &error_message,
                                                      const std::optional<uint64_t> hash) const {
        auto set_value_resp = ProtoConverter::CreateSetVariableValueResponse(success, value, variable, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_set_variable_value() = set_value_resp;

        LOG_INFO("Sending SetVariableValue response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendVariablesChildrenResponse(bool success,
                                                       const std::vector<lldbprotobuf::Variable> &children,
                                                       uint32_t total_children, uint32_t offset, bool has_more,
                                                       const std::string &error_message,
                                                       const std::optional<uint64_t> hash) const {
        auto children_resp = ProtoConverter::CreateVariablesChildrenResponse(
            success, children, total_children, offset, has_more, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_get_variables_children() = children_resp;

        LOG_INFO(
            "Sending VariablesChildren response: success=" + std::to_string(success) + ", children_count=" + std::
            to_string(children.size()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendAddBreakpointResponse(
        bool success,
        BreakpointType breakpoint_type,
        const lldbprotobuf::Breakpoint &breakpoint,
        const std::vector<lldbprotobuf::BreakpointLocation> &locations,
        const std::string &error_message, const std::optional<uint64_t> hash) const {
        // 新版本方法：使用指定的断点类型
        auto bp_resp = ProtoConverter::CreateAddBreakpointResponse(
            success, breakpoint_type, breakpoint, locations, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_add_breakpoint() = bp_resp;

        LOG_INFO("Sending AddBreakpoint response: success=" + std::to_string(success) +
            ", breakpoint_type=" + std::to_string(static_cast<int>(breakpoint_type)) +
            ", breakpoint_id=" + std::to_string(breakpoint.id().id()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendRemoveBreakpointResponse(bool success, const std::string &error_message,
                                                      const std::optional<uint64_t> hash) const {
        auto remove_bp_resp = ProtoConverter::CreateRemoveBreakpointResponse(success, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_remove_breakpoint() = remove_bp_resp;

        LOG_INFO("Sending RemoveBreakpoint response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }


    bool DebuggerClient::ReceiveRequest(lldbprotobuf::Request &request) const {
        return tcp_client_.ReceiveProtoMessage(request);
    }

    bool DebuggerClient::HandleRequest(const lldbprotobuf::Request &request) {
        // Target and Process Management
        if (request.has_create_target()) {
            return HandleCreateTargetRequest(request.create_target(), request.hash());
        }
        if (request.has_launch()) {
            return HandleLaunchRequest(request.launch(), request.hash());
        }
        if (request.has_attach()) {
            return HandleAttachRequest(request.attach(), request.hash());
        }

        if (request.has_detach()) {
            return HandleDetachRequest(request.hash());
        }
        if (request.has_terminate()) {
            return HandleTerminateRequest(request.hash());
        }
        if (request.has_exit()) {
            return HandleExitRequest(request.hash());
        }

        // Execution Control
        if (request.has_continue_()) {
            return HandleContinueRequest(request.hash());
        }
        if (request.has_suspend()) {
            return HandleSuspendRequest(request.hash());
        }
        if (request.has_step_into()) {
            return HandleStepIntoRequest(request.step_into(), request.hash());
        }
        if (request.has_step_over()) {
            return HandleStepOverRequest(request.step_over(), request.hash());
        }
        if (request.has_step_out()) {
            return HandleStepOutRequest(request.step_out(), request.hash());
        }

        // Breakpoints and Watchpoints
        if (request.has_add_breakpoint()) {
            return HandleAddBreakpointRequest(request.add_breakpoint(), request.hash());
        }
        if (request.has_remove_breakpoint()) {
            return HandleRemoveBreakpointRequest(request.remove_breakpoint(), request.hash());
        }


        // Expression Evaluation and Variables


        // Memory and Disassembly


        // Registers


        // Console and Commands


        // Platform and Remote Debugging


        // Thread Control
        if (request.has_threads()) {
            return HandleThreadsRequest(request.threads(), request.hash());
        }

        // Expression Evaluation and Variables
        if (request.has_frames()) {
            return HandleFramesRequest(request.frames(), request.hash());
        }
        if (request.has_variables()) {
            return HandleVariablesRequest(request.variables(), request.hash());
        }
        if (request.has_registers()) {
            return HandleRegistersRequest(request.registers(), request.hash());
        }

        // Expression Evaluation and Variables
        if (request.has_get_value()) {
            return HandleGetValueRequest(request.get_value(), request.hash());
        }

        if (request.has_set_variable_value()) {
            return HandleSetVariableValueRequest(request.set_variable_value(), request.hash());
        }

        if (request.has_get_variables_children()) {
            return HandleVariablesChildrenRequest(request.get_variables_children(), request.hash());
        }

        // Expression Evaluation and Variables
        if (request.has_evaluate()) {
            return HandleEvaluateRequest(request.evaluate(), request.hash());
        }

        // Memory and Disassembly
        if (request.has_read_memory()) {
            return HandleReadMemoryRequest(request.read_memory(), request.hash());
        }

        if (request.has_write_memory()) {
            return HandleWriteMemoryRequest(request.write_memory(), request.hash());
        }

        if (request.has_disassemble()) {
            return HandleDisassembleRequest(request.disassemble(), request.hash());
        }

        // Signal Handling

        // Symbol Download


        LOG_WARNING("Received unknown or unhandled request type");
        return false;
    }

    void DebuggerClient::RunMessageLoop(

        const std::function<bool(const lldbprotobuf::Request &)> &request_handler
    ) {
        LOG_INFO("Starting message loop");

        while (tcp_client_.IsConnected()) {
            lldbprotobuf::Request request;
            if (!tcp_client_.ReceiveProtoMessage(request)) {
                LOG_INFO("Failed to receive request or connection closed");
                break;
            }

            // 检查是否为空消息（size 0），如果是则跳过处理
            if (!request.IsInitialized() || request.ByteSizeLong() == 0) {
                LOG_INFO("Skipping empty request");
                continue;
            }

            LOG_INFO("Received CompositeRequest");

            // 如果提供了请求处理器，调用它；否则使用默认的HandleRequest方法
            if (request_handler) {
                if (!request_handler(request)) {
                    LOG_INFO("Request handler requested loop exit");
                    break;
                }
            } else {
                if (!HandleRequest(request)) {
                    LOG_WARNING("Failed to handle request");
                }
            }
        }

        LOG_INFO("Message loop ended");
    }

    bool DebuggerClient::InitializeLLDB() const {
        if (lldb_initialized_ && debugger_.IsValid()) {
            return true; // 已经初始化
        }

        // 初始化LLDB
        lldb::SBDebugger::Initialize();

        // 创建调试器实例
        debugger_ = lldb::SBDebugger::Create(false);

        if (!debugger_.IsValid()) {
            LOG_ERROR("Failed to create LLDB debugger instance");
            return false;
        }
        event_listener_ = debugger_.GetListener();
        // 设置异步模式
        debugger_.SetAsync(true);

        lldb_initialized_ = true;
        LOG_INFO("LLDB debugger initialized successfully");

        // LLDB 初始化成功后，立即发送 InitializedEvent
        if (!SendInitializedEvent()) {
            LOG_ERROR("Failed to send InitializedEvent after LLDB initialization");
            // 注意：即使发送失败，LLDB 仍然已初始化，所以返回 true
        } else {
            LOG_INFO("InitializedEvent sent successfully");
        }


        return true;
    }

    void DebuggerClient::CleanupLLDB() const {
        LOG_INFO("Cleaning up LLDB resources");

        // 第一步: 确保进程被终止
        if (process_.IsValid()) {
            lldb::StateType state = process_.GetState();
            LOG_INFO("Process state during cleanup: " + std::string(lldb::SBDebugger::StateAsCString(state)));

            // 如果进程还在运行或已停止但未退出，尝试清理
            if (state != lldb::StateType::eStateExited && state != lldb::StateType::eStateDetached) {
                LOG_WARNING("Process is still active during cleanup, attempting termination");

                // 尝试优雅终止
                if (!process_.Destroy()) {
                    LOG_WARNING("Failed to destroy process during cleanup");

                    // 如果销毁失败，尝试分离
                    if (!process_.Detach()) {
                        LOG_ERROR("Failed to detach from process during cleanup");
                    } else {
                        LOG_INFO("Successfully detached from process during cleanup");
                    }
                } else {
                    LOG_INFO("Successfully destroyed process during cleanup");
                }
            }

            // 清理进程引用
            process_ = lldb::SBProcess();
        }

        // 第二步: 清理目标
        if (target_.IsValid()) {
            LOG_INFO("Cleaning up target");
            target_ = lldb::SBTarget();
        }

        // 第三步: 清理断点管理器
        if (breakpoint_manager_) {
            LOG_INFO("Cleaning up breakpoint manager");
            std::string bp_error;
            breakpoint_manager_->ClearAllBreakpoints(bp_error);
        }

        // 第四步: 清理变量映射
        size_t cleaned_vars = CleanupInvalidVariables();
        if (cleaned_vars > 0) {
            LOG_INFO("Cleaned up " + std::to_string(cleaned_vars) + " invalid variables during cleanup");
        }
        variable_id_map_.clear();

        // 第五步: 清理调试器
        if (lldb_initialized_ && debugger_.IsValid()) {
            LOG_INFO("Cleaning up LLDB debugger");
            debugger_ = lldb::SBDebugger();
            lldb::SBDebugger::Terminate();
            lldb_initialized_ = false;
        }

        LOG_INFO("LLDB cleanup completed");
    }

    bool DebuggerClient::HandleTerminateRequest(const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Terminate request");
        if (InitializeLLDB()) {
            if (target_.IsValid()) {
                lldb::SBProcess process = target_.GetProcess();
                if (process.IsValid()) {
                    lldb::SBError error = process.Kill();
                    if (error.Fail()) {
                        LOG_ERROR(std::string("Kill failed: ") + (error.GetCString() ? error.GetCString() : ""));
                    }
                }
            }
        }
        return SendTerminateResponse(hash);
    }

    bool DebuggerClient::HandleCreateTargetRequest(const lldbprotobuf::CreateTargetRequest &req,
                                                   const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling CreateTarget request");
        if (InitializeLLDB()) {
            bool success = false;
            lldb::SBError err;
            target_ = debugger_.CreateTarget(req.file_path().c_str());
            success = target_.IsValid();
            if (!success) {
                LOG_ERROR("Failed to create target");
                return SendCreateTargetResponse(false, "Failed to create target", hash);
            }
        } else {
            return SendCreateTargetResponse(false, "LLDB not available", hash);
        }
        return SendCreateTargetResponse(true, "", hash);
    }


    bool DebuggerClient::HandleLaunchRequest(const lldbprotobuf::LaunchRequest &req,
                                             const std::optional<uint64_t> hash) {
        const lldbprotobuf::ProcessLaunchInfo &launch_info = req.launch_info();

        LOG_INFO("Handling Launch request");
        LOG_INFO("  Executable: " + launch_info.executable_path());
        LOG_INFO("  Working directory: " + launch_info.working_directory());
        LOG_INFO("  Arguments count: " + std::to_string(launch_info.argv_size()));
        LOG_INFO("  Environment count: " + std::to_string(launch_info.env_size()));
        LOG_INFO("  Console mode: " + std::to_string(req.console_mode()));

        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB");
            return SendLaunchResponse(false, -1, "LLDB initialization failed", hash);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available");
            return SendLaunchResponse(false, -1, "No valid target available. Please create a target first.", hash);
        }

        // ============================================
        // 1. 准备命令行参数
        // ============================================
        std::vector<const char *> args;

        // 第一个参数是可执行文件路径
        args.push_back(launch_info.executable_path().c_str());

        // 添加其他命令行参数
        for (int i = 0; i < launch_info.argv_size(); ++i) {
            args.push_back(launch_info.argv(i).c_str());
            LOG_INFO("  Arg[" + std::to_string(i) + "]: " + launch_info.argv(i));
        }

        // 参数列表必须以 nullptr 结尾
        args.push_back(nullptr);

        // ============================================
        // 2. 准备环境变量
        // ============================================
        std::vector<std::string> env_strings; // 存储 "NAME=VALUE" 格式的字符串
        std::vector<const char *> envp;

        for (int i = 0; i < launch_info.env_size(); ++i) {
            const lldbprotobuf::EnvironmentVariable &env_var = launch_info.env(i);
            std::string env_entry = env_var.name() + "=" + env_var.value();
            env_strings.push_back(env_entry);
            LOG_INFO("  Env[" + std::to_string(i) + "]: " + env_entry);
        }

        // 转换为 C 字符串指针数组
        for (const auto &env_str: env_strings) {
            envp.push_back(env_str.c_str());
        }
        envp.push_back(nullptr); // 环境变量列表必须以 nullptr 结尾

        // ============================================
        // 3. 创建 LLDB 启动信息
        // ============================================
        lldb::SBLaunchInfo lldb_launch_info(args.data());

        // 设置工作目录
        if (!launch_info.working_directory().empty()) {
            lldb_launch_info.SetWorkingDirectory(launch_info.working_directory().c_str());
            LOG_INFO("  Working directory set to: " + launch_info.working_directory());
        }

        // 设置环境变量（如果有）
        if (!env_strings.empty()) {
            lldb_launch_info.SetEnvironmentEntries(envp.data(), false); // false = append to existing env
            LOG_INFO("  Environment variables set: " + std::to_string(env_strings.size()) + " entries");
        }

        // ============================================
        // 4. 配置启动标志
        // ============================================
        uint32_t launch_flags = 0;

        LOG_INFO("  Stop at entry: NO (no breakpoints, process will run freely)");


        // 根据 console_mode 设置启动标志
        switch (req.console_mode()) {
            case lldbprotobuf::CONSOLE_MODE_PARENT:
                // 使用父进程的控制台（默认）
                launch_flags |= lldb::eLaunchFlagDisableSTDIO;

                LOG_INFO("  Console mode: PARENT (using LLDB I/O management)");
                break;

            case lldbprotobuf::CONSOLE_MODE_EXTERNAL:
                // 在外部终端中启动
                launch_flags |= lldb::eLaunchFlagLaunchInShell | lldb::eLaunchFlagDisableSTDIO;
                LOG_INFO("  Console mode: EXTERNAL (launch in shell)");
                break;

            case lldbprotobuf::CONSOLE_MODE_PSEUDO:
                // 使用伪终端

                launch_flags |= lldb::eLaunchFlagLaunchInTTY | lldb::eLaunchFlagDisableSTDIO;
                LOG_INFO("  Console mode: PSEUDO (PTY/Named Pipe for bidirectional I/O)");
                break;

            default:
                LOG_WARNING("  Console mode: UNKNOWN (" + std::to_string(req.console_mode()) + "), using PARENT");
                break;
        }

        // 可选：禁用 ASLR（地址空间布局随机化），便于调试

        launch_flags |= lldb::eLaunchFlagDisableASLR;

        lldb_launch_info.SetLaunchFlags(launch_flags);

        // ============================================
        // 5. 处理管道连接，pty
        // ============================================

        // 获取LLDB设置的I/O通道信息
        const std::string &stdin_path = launch_info.stdin_path();
        const std::string &stdout_path = launch_info.stdout_path();
        const std::string &stderr_path = launch_info.stderr_path();


        LOG_INFO("Setting up process I/O management");
        LOG_INFO("  stdin: " + stdin_path);
        LOG_INFO("  stdout: " + stdout_path);
        LOG_INFO("  stderr: " + stderr_path);

        // 如果提供了 I/O 路径，将它们设置到 LLDB launch info 中
        // 这样 LLDB 会自动将进程的 I/O 重定向到这些路径
        if (!stdin_path.empty()) {
            lldb_launch_info.AddOpenFileAction(STDIN_FILENO, stdin_path.c_str(), true, false);
            LOG_INFO("  LLDB stdin redirected to: " + stdin_path);
        }
        if (!stdout_path.empty()) {
            lldb_launch_info.AddOpenFileAction(STDOUT_FILENO, stdout_path.c_str(), false, true);
            LOG_INFO("  LLDB stdout redirected to: " + stdout_path);
        }
        if (!stderr_path.empty()) {
            lldb_launch_info.AddOpenFileAction(STDERR_FILENO, stderr_path.c_str(), false, true);
            LOG_INFO("  LLDB stderr redirected to: " + stderr_path);
        }


        // ============================================
        // 6. 启动进程
        // ============================================
        LOG_INFO("Launching process...");
        lldb::SBError error;
        lldb::SBProcess process = target_.Launch(lldb_launch_info, error);

        // ============================================
        // 7. 检查启动结果
        // ============================================
        if (!process.IsValid() || error.Fail()) {
            std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown launch error";
            LOG_ERROR("Launch failed: " + error_msg);
            return SendLaunchResponse(false, -1, "Launch failed: " + error_msg, hash);
        }

        // 获取进程信息
        lldb::pid_t pid = process.GetProcessID();
        lldb::StateType state = process.GetState();

        LOG_INFO("Process launched successfully!");
        LOG_INFO("  PID: " + std::to_string(pid));
        LOG_INFO("  State: " + std::string(lldb::SBDebugger::StateAsCString(state)));

        // 保存进程引用,以便后续调试操作(Continue, Pause, Step等)可以使用
        process_ = process;


        // ============================================
        // 8. 发送启动成功响应
        // ============================================
        // 必须在事件监控线程启动前发送响应，避免进程快速退出时事件顺序错误
        bool response_sent = SendLaunchResponse(true, static_cast<int64_t>(pid), "", hash);


        // ============================================
        // 9. 启动进程事件监控线程
        // ============================================
        // 启动异步事件监听线程
        // 事件监控会处理：
        // - 断点命中
        // - 进程暂停/继续
        // - 进程退出（正常退出或崩溃）
        // - 异常/信号
        LOG_INFO("Starting async event monitoring thread...");
        StartEventThread();

        return response_sent;
    }


    bool DebuggerClient::HandleAttachRequest(const lldbprotobuf::AttachRequest &req,
                                             const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Attach request for process ID: " + std::to_string(req.process_id().id()));

        // 验证进程ID是否有效
        if (req.process_id().id() <= 0) {
            LOG_ERROR("Invalid process ID: " + std::to_string(req.process_id().id()));
            return SendAttachResponse(false, "Invalid process ID", hash);
        }


        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB");
            return SendAttachResponse(false, "LLDB initialization failed", hash);
        }

        // 创建目标以附加到进程
        lldb::SBError error;
        lldb::SBAttachInfo attach_info(req.process_id().id());

        // LLDB默认在附加后暂停进程，这是安全的行为
        // continue_after_attach参数将在附加成功后处理
        // LOG_INFO("  continue_after_attach: " + std::string(req.continue_after_attach() ? "true" : "false"));

        // 如果没有目标，创建一个空的（用于附加）
        if (!target_.IsValid()) {
            target_ = debugger_.CreateTarget("", nullptr, nullptr, false, error);
            if (!target_.IsValid()) {
                LOG_ERROR(
                    "Failed to create target for attach: " + std::string(error.GetCString() ? error.GetCString() : ""));
                return SendAttachResponse(false, "Failed to create target for attach", hash);
            }
        }

        // 附加到指定进程
        lldb::SBProcess process = target_.Attach(attach_info, error);
        if (!process.IsValid() || error.Fail()) {
            std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown attach error";
            LOG_ERROR("Failed to attach to process " + std::to_string(req.process_id().id()) + ": " + error_msg);
            return SendAttachResponse(false, "Failed to attach to process: " + error_msg, hash);
        }

        // 获取进程信息
        lldb::pid_t pid = process.GetProcessID();
        lldb::StateType state = process.GetState();

        LOG_INFO("Successfully attached to process!");
        LOG_INFO("  PID: " + std::to_string(pid));
        LOG_INFO("  State: " + std::string(lldb::SBDebugger::StateAsCString(state)));

        // 验证附加后进程状态
        if (state == lldb::eStateExited || state == lldb::eStateCrashed) {
            int exit_code = process.GetExitStatus();
            std::string exit_desc = process.GetExitDescription() ? process.GetExitDescription() : "No description";

            LOG_ERROR("Process terminated after attach");
            LOG_ERROR("  Exit code: " + std::to_string(exit_code));
            LOG_ERROR("  Exit description: " + exit_desc);

            return SendAttachResponse(false, "Process terminated after attach (exit code: " +
                                             std::to_string(exit_code) + ", " + exit_desc + ")", hash);
        }


        return SendAttachResponse(true, "", hash);
    }


    bool DebuggerClient::HandleDetachRequest(const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Detach request");

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process to detach");
            return SendDetachResponse(false, "No valid process to detach", hash);
        }

        // 执行分离操作
        lldb::SBError error = process_.Detach();
        if (error.Fail()) {
            std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown detach error";
            LOG_ERROR("Failed to detach process: " + error_msg);
            return SendDetachResponse(false, "Detach failed: " + error_msg, hash);
        }

        LOG_INFO("Successfully detached from process");

        // 清理进程引用
        process_ = lldb::SBProcess();

        return SendDetachResponse(true, "", hash);
    }

    bool DebuggerClient::HandleExitRequest(const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Exit request");

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_WARNING("No valid process to exit - debugger will exit anyway");
            // 即使没有进程，也要发送响应，因为这是调试器退出请求
            return SendExitResponse(hash);
        }

        lldb::StateType state = process_.GetState();
        LOG_INFO("Current process state: " + std::string(lldb::SBDebugger::StateAsCString(state)));

        // 如果进程已经退出或崩溃，无需额外操作
        if (state == lldb::eStateExited || state == lldb::eStateCrashed) {
            LOG_INFO("Process already exited or crashed, proceeding with debugger exit");
            return SendExitResponse(hash);
        }

        // 尝试优雅地退出进程
        // 首先尝试发送SIGTERM信号（如果是Unix系统）
        // 在Windows上，Kill()是主要的退出方法
        lldb::SBError error = process_.Kill();

        if (error.Fail()) {
            std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown error during process exit";
            LOG_ERROR("Failed to kill process: " + error_msg);
            // 即使Kill失败，也尝试Detach来清理资源
            lldb::SBError detach_error = process_.Detach();
            if (detach_error.Fail()) {
                LOG_ERROR(
                    "Failed to detach process: " + std::string(detach_error.GetCString() ? detach_error.GetCString() :
                        ""));
            }
        } else {
            LOG_INFO("Process killed successfully");
        }

        // 清理进程引用
        process_ = lldb::SBProcess();

        return SendExitResponse(hash);
    }

    bool DebuggerClient::HandleContinueRequest(const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Continue request");

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process to continue");
            return SendContinueResponse(hash);
        }


        // 继续执行进程
        lldb::SBError error = process_.Continue();
        if (error.Fail()) {
            LOG_ERROR("Failed to continue process: " + std::string(error.GetCString()));
            return SendContinueResponse(hash);
        }

        // 事件由事件监听线程异步处理
        return SendContinueResponse(hash);
    }

    bool DebuggerClient::HandleSuspendRequest(const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Suspend request");

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process to suspend");
            return SendSuspendResponse(hash);
        }

        lldb::StateType state = process_.GetState();
        LOG_INFO("Current process state: " + std::string(lldb::SBDebugger::StateAsCString(state)));

        // 如果进程已经停止或退出，无需额外操作
        if (state == lldb::eStateStopped || state == lldb::eStateSuspended ||
            state == lldb::eStateExited || state == lldb::eStateCrashed) {
            LOG_INFO("Process already stopped/suspended/exited/crashed");
            return SendSuspendResponse(hash);
        }

        // 如果进程正在运行，暂停它
        // 使用 Signal() 发送 SIGSTOP (Unix) 或等效的暂停信号
        // 在Windows上，SIGSTOP可能不可用，我们使用信号值19（SIGSTOP的Unix值）
        lldb::SBError error = process_.Signal(19); // SIGSTOP signal number

        if (error.Fail()) {
            std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown error during process suspend";
            LOG_ERROR("Failed to suspend process: " + error_msg);
            // 即使信号发送失败，也尝试使用 Stop() 方法
            error = process_.Stop();
            if (error.Fail()) {
                LOG_ERROR("Failed to stop process with Stop() method: " +
                    std::string(error.GetCString() ? error.GetCString() : ""));
            } else {
                LOG_INFO("Process stopped successfully using Stop() method");
            }
        } else {
            LOG_INFO("Process suspend signal sent successfully");
        }

        return SendSuspendResponse(hash);
    }

    bool DebuggerClient::HandleStepIntoRequest(const lldbprotobuf::StepIntoRequest &req,
                                               const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling StepInto request for thread ID: " + std::to_string(req.thread_id().id()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for step into");
            return SendStepIntoResponse(false, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found for step into: " + std::to_string(req.thread_id().id()));
            return SendStepIntoResponse(false, "Thread not found", hash);
        }

        // 设置当前线程
        process_.SetSelectedThread(target_thread);

        // 执行单步进入 (step into)
        try {
            target_thread.StepInto(lldb::eOnlyDuringStepping);
            LOG_INFO("StepInto initiated successfully for thread " + std::to_string(req.thread_id().id()));
            return SendStepIntoResponse(true, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("StepInto failed for thread " + std::to_string(req.thread_id().id()) + ": " + e.what());
            return SendStepIntoResponse(false, "Step into failed: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("StepInto failed with unknown error for thread " + std::to_string(req.thread_id().id()));
            return SendStepIntoResponse(false, "Step into operation failed", hash);
        }
    }

    bool DebuggerClient::HandleStepOverRequest(const lldbprotobuf::StepOverRequest &req,
                                               const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling StepOver request for thread ID: " + std::to_string(req.thread_id().id()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for step over");
            return SendStepOverResponse(false, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found for step over: " + std::to_string(req.thread_id().id()));
            return SendStepOverResponse(false, "Thread not found", hash);
        }

        // 设置当前线程
        process_.SetSelectedThread(target_thread);

        // 执行单步跳过 (step over)
        try {
            lldb::SBError error;
            target_thread.StepOver(lldb::eOnlyDuringStepping, error);

            if (error.Fail()) {
                std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown error";
                LOG_ERROR("Failed to step over thread " + std::to_string(req.thread_id().id()) + ": " + error_msg);
                return SendStepOverResponse(false, "Step over failed: " + error_msg, hash);
            }

            LOG_INFO("StepOver initiated successfully for thread " + std::to_string(req.thread_id().id()));
            return SendStepOverResponse(true, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("StepOver failed for thread " + std::to_string(req.thread_id().id()) + ": " + e.what());
            return SendStepOverResponse(false, "Step over failed: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("StepOver failed with unknown error for thread " + std::to_string(req.thread_id().id()));
            return SendStepOverResponse(false, "Step over operation failed", hash);
        }
    }

    bool DebuggerClient::HandleStepOutRequest(const lldbprotobuf::StepOutRequest &req,
                                              const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling StepOut request for thread ID: " + std::to_string(req.thread_id().id()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for step out");
            return SendStepOutResponse(false, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found for step out: " + std::to_string(req.thread_id().id()));
            return SendStepOutResponse(false, "Thread not found", hash);
        }

        // 设置当前线程
        process_.SetSelectedThread(target_thread);

        // 执行单步跳出 (step out)
        try {
            target_thread.StepOut();
            LOG_INFO("StepOut initiated successfully for thread " + std::to_string(req.thread_id().id()));
            return SendStepOutResponse(true, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("StepOut failed for thread " + std::to_string(req.thread_id().id()) + ": " + e.what());
            return SendStepOutResponse(false, "Step out failed: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("StepOut failed with unknown error for thread " + std::to_string(req.thread_id().id()));
            return SendStepOutResponse(false, "Step out operation failed", hash);
        }
    }


    bool DebuggerClient::HandleAddBreakpointRequest(const lldbprotobuf::AddBreakpointRequest &req,
                                                    const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling AddBreakpoint request");

        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB");
            lldbprotobuf::Breakpoint bp;
            std::vector<lldbprotobuf::BreakpointLocation> locations;
            return SendAddBreakpointResponse(false, BreakpointType::LINE_BREAKPOINT, bp, locations,
                                             "LLDB not available", hash);
        }

        // 设置 BreakpointManager 的 target
        if (target_.IsValid()) {
            breakpoint_manager_->SetTarget(target_);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available");
            lldbprotobuf::Breakpoint bp;
            std::vector<lldbprotobuf::BreakpointLocation> locations;
            return SendAddBreakpointResponse(false, BreakpointType::LINE_BREAKPOINT, bp, locations,
                                             "No valid target available", hash);
        }

        // 使用 BreakpointManager 处理断点请求
        auto create_result = breakpoint_manager_->HandleAddBreakpointRequest(req);

        if (!create_result.success) {
            LOG_ERROR("Failed to create breakpoint: " + create_result.breakpoint_info.error_message);
            lldbprotobuf::Breakpoint bp;
            std::vector<lldbprotobuf::BreakpointLocation> locations;
            return SendAddBreakpointResponse(false, create_result.breakpoint_info.type, bp, locations,
                                             create_result.breakpoint_info.error_message, hash);
        }


        // 创建 proto 断点对象
        lldbprotobuf::Breakpoint proto_bp;
        lldbprotobuf::SourceLocation original_location;

        // 设置原始位置信息
        switch (create_result.breakpoint_info.type) {
            case BreakpointType::LINE_BREAKPOINT:
                original_location = ProtoConverter::CreateSourceLocation(
                    create_result.breakpoint_info.file_path,
                    create_result.breakpoint_info.line_number);
                break;
            case BreakpointType::ADDRESS_BREAKPOINT:
                original_location = ProtoConverter::CreateSourceLocation("", 0);
                break;
            case BreakpointType::FUNCTION_BREAKPOINT:
                original_location = ProtoConverter::CreateSourceLocation(
                    create_result.breakpoint_info.function_name, 0);
                break;
            case BreakpointType::SYMBOL_BREAKPOINT:
                original_location = ProtoConverter::CreateSourceLocation(
                    create_result.breakpoint_info.symbol_pattern, 0);
                break;
            default:
                original_location = ProtoConverter::CreateSourceLocation("", 0);
                break;
        }

        // 创建 proto 断点对象
        proto_bp = ProtoConverter::CreateBreakpoint(
            create_result.breakpoint_info.lldb_id,
            original_location,
            create_result.breakpoint_info.condition
        );

        // 转换位置信息 - 从 unique_ptr 转换为值
        std::vector<lldbprotobuf::BreakpointLocation> locations;
        for (const auto &loc: create_result.locations) {
            if (loc) {
                locations.push_back(*loc.get());
            }
        }

        LOG_INFO("Breakpoint created successfully!");
        LOG_INFO("  Breakpoint ID: " + std::to_string(create_result.breakpoint_info.lldb_id));
        LOG_INFO("  Breakpoint Type: " + std::to_string(static_cast<int>(create_result.breakpoint_info.type)));
        LOG_INFO("  Locations count: " + std::to_string(locations.size()));

        return SendAddBreakpointResponse(true, create_result.breakpoint_info.type, proto_bp, locations, "", hash);
    }

    bool DebuggerClient::HandleRemoveBreakpointRequest(const lldbprotobuf::RemoveBreakpointRequest &req,
                                                       const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling RemoveBreakpoint request");

        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB");
            return SendRemoveBreakpointResponse(false, "LLDB not available", hash);
        }

        // 设置 BreakpointManager 的 target
        if (target_.IsValid()) {
            breakpoint_manager_->SetTarget(target_);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available");
            return SendRemoveBreakpointResponse(false, "No valid target available", hash);
        }

        // 使用 BreakpointManager 处理删除断点请求
        std::string error_message;

        if (breakpoint_manager_->HandleRemoveBreakpointRequest(req, error_message)) {
            LOG_INFO("Breakpoint removed successfully!");
            return SendRemoveBreakpointResponse(true, "", hash);
        } else {
            LOG_ERROR("Failed to remove breakpoint: " + error_message);
            return SendRemoveBreakpointResponse(false, error_message, hash);
        }
    }

    bool DebuggerClient::HandleThreadsRequest(const lldbprotobuf::ThreadsRequest &req,
                                              const std::optional<uint64_t> hash) const {
        (void) req; // 当前请求没有参数需要处理
        LOG_INFO("Handling Threads request");

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendThreadsResponse(false, {}, "No valid process available", hash);
        }

        std::vector<lldbprotobuf::Thread> threads;

        // 获取进程中的所有线程
        uint32_t num_threads = process_.GetNumThreads();
        LOG_INFO("Process has " + std::to_string(num_threads) + " threads");

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid()) {
                // 使用 ProtoConverter 将 LLDB 线程转换为 protobuf 线程
                lldbprotobuf::Thread thread = ProtoConverter::CreateThread(sb_thread);
                threads.push_back(thread);

                LOG_INFO("  Thread " + std::to_string(i) + ": ID=" + std::to_string(sb_thread.GetThreadID()) +
                    ", Name=" + std::string(sb_thread.GetName() ? sb_thread.GetName() : "unnamed"));
            }
        }

        LOG_INFO("Successfully retrieved " + std::to_string(threads.size()) + " threads");
        return SendThreadsResponse(true, threads, "", hash);
    }

    bool DebuggerClient::HandleFramesRequest(const lldbprotobuf::FramesRequest &req,
                                             const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Frames request: thread_id=" + std::to_string(req.thread_id().id()) +
            ", start_frame=" + std::to_string(req.start_index()) +
            ", count=" + std::to_string(req.count()) +
            ", first_valid_source_only=" + std::to_string(req.first_valid_source_only()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendFramesResponse(false, {}, 0, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found: " + std::to_string(req.thread_id().id()));
            return SendFramesResponse(false, {}, 0, "Thread not found", hash);
        }

        std::vector<lldbprotobuf::Frame> frames;
        uint32_t total_frames = target_thread.GetNumFrames();

        // 辅助函数：检查帧是否有有效的源码行信息
        auto hasValidSourceLine = [](const lldb::SBFrame &frame) -> bool {
            if (!frame.IsValid()) {
                return false;
            }

            lldb::SBLineEntry line_entry = frame.GetLineEntry();
            if (!line_entry.IsValid()) {
                return false;
            }

            // 检查是否有有效的文件名和行号
            const char *filename = line_entry.GetFileSpec().GetFilename();
            uint32_t line = line_entry.GetLine();

            return (filename != nullptr && strlen(filename) > 0 && line > 0);
        };

        if (req.first_valid_source_only()) {
            // 只返回第一个有效的源码帧
            LOG_INFO("Searching for first frame with valid source information");

            uint32_t start_idx = req.start_index();
            uint32_t search_end = std::min(start_idx + req.count(), total_frames);

            for (uint32_t i = start_idx; i < search_end; ++i) {
                lldb::SBFrame sb_frame = target_thread.GetFrameAtIndex(i);
                if (sb_frame.IsValid() && hasValidSourceLine(sb_frame)) {
                    lldbprotobuf::Frame frame = ProtoConverter::CreateFrame(sb_frame);
                    frames.push_back(frame);

                    lldb::SBLineEntry line_entry = sb_frame.GetLineEntry();
                    LOG_INFO("Found first valid source frame at index " + std::to_string(i) +
                        ": " + std::string(sb_frame.GetFunctionName() ? sb_frame.GetFunctionName() : "unknown") +
                        " at " + std::string(line_entry.GetFileSpec().GetFilename() ? line_entry.GetFileSpec().
                            GetFilename() : "unknown") +
                        ":" + std::to_string(line_entry.GetLine()));
                    break;
                }
            }

            if (frames.empty()) {
                LOG_INFO("No frame with valid source information found in requested range");
            }
        } else {
            // 返回请求范围内的所有帧
            uint32_t start_idx = req.start_index();
            uint32_t end_idx = std::min(start_idx + req.count(), total_frames);

            LOG_INFO("Thread has " + std::to_string(total_frames) + " total frames, returning frames " +
                std::to_string(start_idx) + " to " + std::to_string(end_idx));

            for (uint32_t i = start_idx; i < end_idx; ++i) {
                lldb::SBFrame sb_frame = target_thread.GetFrameAtIndex(i);
                if (sb_frame.IsValid()) {
                    lldbprotobuf::Frame frame = ProtoConverter::CreateFrame(sb_frame);
                    frames.push_back(frame);

                    LOG_INFO(
                        "  Frame " + std::to_string(i) + ": " + std::string(sb_frame.GetFunctionName() ? sb_frame.
                            GetFunctionName() : "unknown"));
                }
            }
        }

        LOG_INFO("Successfully retrieved " + std::to_string(frames.size()) + " frames");
        return SendFramesResponse(true, frames, total_frames, "", hash);
    }

    bool DebuggerClient::HandleVariablesRequest(const lldbprotobuf::VariablesRequest &req,
                                                const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Variables request: thread_id=" + std::to_string(req.thread_id().id()) +
            ", frame_index=" + std::to_string(req.frame_index()) +
            ", include_arguments=" + std::to_string(req.include_arguments()) +
            ", include_locals=" + std::to_string(req.include_locals()) +
            ", include_statics=" + std::to_string(req.include_statics()) +
            ", in_scope_only=" + std::to_string(req.in_scope_only()) +
            ", include_runtime_support_values=" + std::to_string(req.include_runtime_support_values()) +
            ", use_dynamic=" + std::to_string(static_cast<int>(req.use_dynamic())) +
            ", include_recognized_arguments=" + std::to_string(req.include_recognized_arguments()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendVariablesResponse(false, {}, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found: " + std::to_string(req.thread_id().id()));
            return SendVariablesResponse(false, {}, "Thread not found", hash);
        }

        // 获取指定的栈帧
        uint32_t num_frames = target_thread.GetNumFrames();

        if (req.frame_index() >= num_frames) {
            LOG_ERROR(
                "Frame index out of range: " + std::to_string(req.frame_index()) + " >= " + std::to_string(num_frames));
            return SendVariablesResponse(false, {}, "Frame index out of range", hash);
        }

        lldb::SBFrame target_frame = target_thread.GetFrameAtIndex(req.frame_index());
        if (!target_frame.IsValid()) {
            LOG_ERROR("Invalid frame at index " + std::to_string(req.frame_index()));
            return SendVariablesResponse(false, {}, "Invalid frame", hash);
        }

        std::vector<lldbprotobuf::Variable> variables;

        // 创建变量选项对象
        lldb::SBVariablesOptions var_options;
        if (!var_options.IsValid()) {
            LOG_ERROR("Failed to create SBVariablesOptions");
            return SendVariablesResponse(false, {}, "Failed to create variables options", hash);
        }

        // 配置变量选项（将 protobuf 字段映射到 SBVariablesOptions）
        var_options.SetIncludeArguments(req.include_arguments());
        var_options.SetIncludeLocals(req.include_locals());
        var_options.SetIncludeStatics(req.include_statics());
        var_options.SetInScopeOnly(req.in_scope_only());
        var_options.SetIncludeRuntimeSupportValues(req.include_runtime_support_values());
        var_options.SetIncludeRecognizedArguments(req.include_recognized_arguments());

        // 将 protobuf 枚举转换为 LLDB DynamicValueType
        lldb::DynamicValueType dynamic_type = lldb::eNoDynamicValues;
        switch (req.use_dynamic()) {
            case lldbprotobuf::DYNAMIC_VALUE_NONE:
                dynamic_type = lldb::eNoDynamicValues;
                break;
            case lldbprotobuf::DYNAMIC_VALUE_DONT_RUN_TARGET:
                dynamic_type = lldb::eDynamicDontRunTarget;
                break;
            case lldbprotobuf::DYNAMIC_VALUE_RUN_TARGET:
                dynamic_type = lldb::eDynamicCanRunTarget;
                break;
            default:
                LOG_WARNING(
                    "Unknown dynamic value type: " + std::to_string(static_cast<int>(req.use_dynamic())) +
                    ", using eNoDynamicValues");
                dynamic_type = lldb::eNoDynamicValues;
                break;
        }
        var_options.SetUseDynamic(dynamic_type);

        // 使用配置好的选项获取变量
        lldb::SBValueList local_vars = target_frame.GetVariables(var_options);

        LOG_INFO(
            "Found " + std::to_string(local_vars.GetSize()) + " variables in frame " + std::to_string(req.frame_index()
            ));

        // 转换 LLDB SBValue 到 protobuf Variable
        for (uint32_t i = 0; i < local_vars.GetSize(); ++i) {
            lldb::SBValue sb_value = local_vars.GetValueAtIndex(i);

            if (!sb_value.IsValid()) {
                continue;
            }

            try {
                // 为变量分配唯一的ID并存储映射
                uint64_t variable_id = AllocateVariableId(req.thread_id().id(), req.frame_index(), sb_value);

                // 使用 ProtoConverter 将 LLDB 变量转换为 protobuf 变量
                lldbprotobuf::Variable proto_var = ProtoConverter::CreateVariable(sb_value, variable_id);


                variables.push_back(proto_var);

                LOG_INFO("  Variable: " + std::string(sb_value.GetName() ? sb_value.GetName() : "unnamed") +
                    " (ID=" + std::to_string(variable_id) + ") (" +
                    std::string(sb_value.GetTypeName() ? sb_value.GetTypeName() : "unknown") + ") = " +
                    std::string(sb_value.GetValue() ? sb_value.GetValue() : "<no value>"));
            } catch (const std::exception &e) {
                LOG_WARNING("Failed to convert variable at index " + std::to_string(i) + ": " + e.what());
                // 继续处理其他变量，不要因为一个失败而中断
            }
        }

  
        LOG_INFO(
            "Successfully extracted " + std::to_string(variables.size()) +
            " variables (including arguments and locals)");
        return SendVariablesResponse(true, variables, "", hash);
    }

    bool DebuggerClient::HandleRegistersRequest(const lldbprotobuf::RegistersRequest &req,
                                               const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Registers request: thread_id=" + std::to_string(req.thread_id().id()) +
            ", frame_index=" + std::to_string(req.frame_index()) +
            ", include_detailed_values=" + std::to_string(req.include_detailed_values()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendRegistersResponse(false, {}, {}, false, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        uint32_t num_threads = process_.GetNumThreads();
        bool thread_found = false;

        for (uint32_t i = 0; i < num_threads; ++i) {
            lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
            if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                target_thread = sb_thread;
                thread_found = true;
                break;
            }
        }

        if (!thread_found) {
            LOG_ERROR("Thread not found: " + std::to_string(req.thread_id().id()));
            return SendRegistersResponse(false, {}, {}, false, "Thread not found", hash);
        }

        // 获取指定的栈帧
        uint32_t num_frames = target_thread.GetNumFrames();

        if (req.frame_index() >= num_frames) {
            LOG_ERROR(
                "Frame index out of range: " + std::to_string(req.frame_index()) + " >= " + std::to_string(num_frames));
            return SendRegistersResponse(false, {}, {}, false, "Frame index out of range", hash);
        }

        lldb::SBFrame target_frame = target_thread.GetFrameAtIndex(req.frame_index());
        if (!target_frame.IsValid()) {
            LOG_ERROR("Invalid frame at index " + std::to_string(req.frame_index()));
            return SendRegistersResponse(false, {}, {}, false, "Invalid frame", hash);
        }

        // 获取寄存器上下文
        lldb::SBValueList reg_vars = target_frame.GetRegisters();
        if (!reg_vars.IsValid()) {
            LOG_ERROR("Failed to get register context for frame " + std::to_string(req.frame_index()));
            return SendRegistersResponse(false, {}, {}, false, "Failed to get register context", hash);
        }

        std::vector<lldbprotobuf::Register> registers;
        std::vector<lldbprotobuf::RegisterGroup> register_groups;

        LOG_INFO("Found " + std::to_string(reg_vars.GetSize()) + " registers in frame " + std::to_string(req.frame_index()));

        // 处理寄存器组过滤
        std::set<std::string> requested_groups;
        for (const std::string& group : req.register_groups()) {
            requested_groups.insert(group);
        }

        // 处理寄存器名称过滤
        std::set<std::string> requested_names;
        for (const std::string& name : req.register_names()) {
            requested_names.insert(name);
        }

        // 转换寄存器
        for (uint32_t i = 0; i < reg_vars.GetSize(); ++i) {
            lldb::SBValue reg_value = reg_vars.GetValueAtIndex(i);
            if (!reg_value.IsValid()) {
                continue;
            }

            const char* reg_name = reg_value.GetName();
            if (!reg_name) {
                continue;
            }

            // 检查寄存器名称过滤
            if (!requested_names.empty() && requested_names.find(reg_name) == requested_names.end()) {
                continue;
            }

            // 检查寄存器组过滤
            const char* reg_set_name = reg_value.GetValueForExpressionPath(".register-set").GetValue();
            std::string register_set = reg_set_name ? reg_set_name : "general";

            if (!requested_groups.empty() && requested_groups.find(register_set) == requested_groups.end()) {
                continue;
            }

            try {
                // 分配寄存器ID
                uint64_t register_id = AllocateVariableId(req.thread_id().id(), req.frame_index(), reg_value);

                // 创建寄存器protobuf对象
                lldbprotobuf::Register proto_register = ProtoConverter::CreateRegister(reg_value, register_id);

                // 设置寄存器组信息
                proto_register.set_register_set(register_set);

                registers.push_back(proto_register);

                LOG_INFO("  Register: " + std::string(reg_name) +
                    " (group: " + register_set + ") = " +
                    std::string(reg_value.GetValue() ? reg_value.GetValue() : "<no value>"));
            } catch (const std::exception &e) {
                LOG_WARNING("Failed to convert register at index " + std::to_string(i) + ": " + e.what());
                // 继续处理其他寄存器，不要因为一个失败而中断
            }
        }

        // 创建寄存器组信息
        std::map<std::string, std::vector<uint32_t>> group_map;
        for (uint32_t i = 0; i < registers.size(); ++i) {
            const std::string& group_name = registers[i].register_set();
            group_map[group_name].push_back(i);
        }

        for (const auto& group_pair : group_map) {
            lldbprotobuf::RegisterGroup group;
            group.set_name(group_pair.first);
            group.set_description("Register group: " + group_pair.first);
            group.set_register_count(group_pair.second.size());
            group.set_visible(true);
            register_groups.push_back(group);
        }

        LOG_INFO("Successfully extracted " + std::to_string(registers.size()) +
                " registers from " + std::to_string(register_groups.size()) + " groups");
        return SendRegistersResponse(true, registers, register_groups, req.include_detailed_values(), "", hash);
    }

    bool DebuggerClient::SendRegistersResponse(bool success,
                                              const std::vector<lldbprotobuf::Register> &registers,
                                              const std::vector<lldbprotobuf::RegisterGroup> &register_groups,
                                              bool include_detailed_values,
                                              const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto registers_resp = ProtoConverter::CreateRegistersResponse(success, registers, register_groups,
                                                                    include_detailed_values, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_registers() = registers_resp;

        LOG_INFO("Sending Registers response: success=" + std::to_string(success) +
                ", register_count=" + std::to_string(registers.size()));
        return tcp_client_.SendProtoMessage(response);
    }

    // ============================================================================
    // Variable ID Management Implementation
    // ============================================================================

    lldb::SBValue DebuggerClient::FindVariableById(uint64_t variable_id) const {
        auto it = variable_id_map_.find(variable_id);
        if (it == variable_id_map_.end()) {
            LOG_ERROR("Variable ID not found in mapping: " + std::to_string(variable_id));
            return lldb::SBValue();
        }

        // 检查SBValue对象是否仍然有效
        lldb::SBValue sb_value = it->second;
        if (!sb_value.IsValid()) {
            LOG_WARNING(
                "Found variable ID " + std::to_string(variable_id) + " but SBValue is invalid, removing from map");
            variable_id_map_.erase(it);
            return lldb::SBValue();
        }

        return sb_value;
    }

    uint64_t DebuggerClient::AllocateVariableId(uint64_t thread_id, uint32_t frame_index,
                                                lldb::SBValue &sb_value) const {
        if (!sb_value.IsValid()) {
            LOG_ERROR("Cannot allocate ID for invalid SBValue");
            return 0;
        }

        // 获取变量的多个属性用于生成更唯一的哈希
        const char *variable_name = sb_value.GetName();
        const char *type_name = sb_value.GetTypeName();
        uint64_t address = sb_value.GetAddress().GetLoadAddress(target_);
        uint64_t byte_size = sb_value.GetByteSize();
        auto current_time = std::chrono::high_resolution_clock::now();
        auto time_stamp = current_time.time_since_epoch().count();

        // 组合多个变量属性生成哈希
        std::hash<std::string> str_hasher;
        std::hash<uint64_t> num_hasher;

        // 组合变量名称、类型名称、地址、大小、线程ID、帧索引和时间戳
        uint64_t name_hash = variable_name ? str_hasher(std::string(variable_name)) : 0;
        uint64_t type_hash = type_name ? str_hasher(std::string(type_name)) : 0;
        uint64_t combined = num_hasher(
            thread_id ^
            (static_cast<uint64_t>(frame_index) << 32) ^
            address ^
            byte_size ^
            time_stamp ^
            name_hash ^
            type_hash
        );

        // 再进行一次哈希以确保更好的分布
        uint64_t variable_id = num_hasher(combined + 0x9e3779b97f4a7c15); // 黄金比例常数增加随机性

        // 确保ID不为0且为正数
        if (variable_id == 0) {
            variable_id = 1;
        }

        // 直接存储SBValue对象
        variable_id_map_[variable_id] = sb_value;

        // 获取变量名称用于日志
        if (!variable_name) {
            variable_name = "<unnamed>";
        }

        LOG_INFO("Allocated variable ID " + std::to_string(variable_id) + " for variable '" +
            std::string(variable_name) + "' (type: " + std::string(type_name ? type_name : "unknown") +
            ", addr: 0x" + std::to_string(address) + ", size: " + std::to_string(byte_size) +
            ") in thread " + std::to_string(thread_id) + ", frame " + std::to_string(frame_index));

        return variable_id;
    }

    size_t DebuggerClient::CleanupInvalidVariables() const {
        size_t cleaned_count = 0;
        auto it = variable_id_map_.begin();

        while (it != variable_id_map_.end()) {
            if (!it->second.IsValid()) {
                // 记录清理的变量信息
                const char *var_name = it->second.GetName();
                if (!var_name) {
                    var_name = "<unnamed>";
                }

                LOG_INFO("Cleaning up invalid variable '" + std::string(var_name) +
                    "' with ID " + std::to_string(it->first));

                it = variable_id_map_.erase(it);
                cleaned_count++;
            } else {
                ++it;
            }
        }

        if (cleaned_count > 0) {
            LOG_INFO("Cleaned up " + std::to_string(cleaned_count) + " invalid variables, " +
                "remaining: " + std::to_string(variable_id_map_.size()));
        }

        return cleaned_count;
    }


    // ============================================================================
    // Process Event Monitoring - Event Thread Implementation
    // ============================================================================

    void DebuggerClient::StartEventThread() {
        // 如果事件线程已经在运行,不要重复启动
        if (event_thread_running_.load()) {
            LOG_WARNING("Event thread is already running");
            return;
        }

        // 设置运行标志
        event_thread_running_.store(true);

        // 设置所有事件监听器
        SetupAllEventListeners();

        // 启动事件监听线程
        event_thread_ = std::thread(&DebuggerClient::EventThreadLoop, this);

        LOG_INFO("Event monitoring thread started");
    }

    void DebuggerClient::StopEventThread() {
        // 设置停止标志
        event_thread_running_.store(false);

        // 等待线程退出
        if (event_thread_.joinable()) {
            event_thread_.join();
            LOG_INFO("Event monitoring thread stopped");
        }
    }

    void DebuggerClient::EventThreadLoop() {
        LOG_INFO("Event thread loop started");

        if (!event_listener_.IsValid()) {
            LOG_ERROR("Event listener is not valid in event thread");
            event_thread_running_.store(false);
            return;
        }

        LOG_INFO("Event thread initialized, starting comprehensive event monitoring loop");

        // 事件监听主循环
        while (event_thread_running_.load()) {
            lldb::SBEvent event;

            // 等待事件(1秒超时)
            if (!event_listener_.WaitForEvent(1, event)) {
                // 超时,检查进程状态
                if (process_.IsValid()) {
                    lldb::StateType state = process_.GetState();

                    // 只在状态变化时记录
                    if (state == lldb::eStateExited || state == lldb::eStateCrashed) {
                        if (state == lldb::eStateExited) {
                            int exit_code = process_.GetExitStatus();
                            std::string exit_desc = process_.GetExitDescription() ? process_.GetExitDescription() : "";
                            LOG_INFO("Process exited with code: " + std::to_string(exit_code));
                        } else {
                            LOG_ERROR("Process crashed");
                        }
                        // 进程已终止,可以退出事件循环
                        break;
                    }
                }
                continue;
            }


            // 收到事件,使用综合事件处理器
            HandleEvent(event);
        }

        LOG_INFO("Event thread loop ended");
        event_thread_running_.store(false);
    }

    // ========================================================================
    // Event Broadcasting Methods Implementation
    // ========================================================================

    bool DebuggerClient::SendProcessExitedEvent(int32_t exit_code, const std::string &exit_description) const {
        auto process_exited_event = ProtoConverter::CreateProcessExitedEvent(
            exit_code,
            exit_description
        );

        lldbprotobuf::Event event;
        *event.mutable_process_exited() = process_exited_event;

        LOG_INFO("Broadcasting ProcessExited event: exit_code=" + std::to_string(exit_code) +
            ", description=" + exit_description);
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendProcessStoppedEvent(lldb::SBThread &thread) const {
        // Get the current frame from the thread
        lldb::SBFrame frame = thread.GetFrameAtIndex(0);
        if (!frame.IsValid()) {
            LOG_ERROR("Cannot create ProcessStopped event: thread has no valid frame");
            return false;
        }

        // Use ProtoConverter to create ProcessStopped event
        lldbprotobuf::ProcessStopped process_stopped = ProtoConverter::CreateProcessStopped(thread, frame);

        // Create the event and send it
        lldbprotobuf::Event event;
        *event.mutable_process_stopped() = process_stopped;

        LOG_INFO("Broadcasting ProcessStopped event: thread_id=" + std::to_string(thread.GetThreadID()));
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendProcessOutputEvent(const std::string &text, lldbprotobuf::OutputType output_type) const {
        // Use ProtoConverter to create ProcessOutput event
        lldbprotobuf::ProcessOutput process_output = ProtoConverter::CreateProcessOutputEvent(text, output_type);

        // Create the event and send it
        lldbprotobuf::Event event;
        *event.mutable_process_output() = process_output;

        LOG_INFO(
            "Broadcasting ProcessOutput event: type=" + std::to_string(output_type) + ", length=" + std::to_string(text.
                length()));
        return tcp_client_.SendEventBroadcast(event);
    }

    // 新增事件发送函数实现
    bool DebuggerClient::SendModuleLoadedEvent(const std::vector<lldbprotobuf::Module> &modules) const {
        if (modules.empty()) {
            return true; // 没有模块需要发送
        }

        lldbprotobuf::ModuleEvent module_event = ProtoConverter::CreateModuleLoadedEvent(modules);

        lldbprotobuf::Event event;
        *event.mutable_module_event() = module_event;

        LOG_INFO("Broadcasting ModuleLoaded event: " + std::to_string(modules.size()) + " modules");
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendModuleUnloadedEvent(const std::vector<lldbprotobuf::Module> &modules) const {
        if (modules.empty()) {
            return true; // 没有模块需要发送
        }

        lldbprotobuf::ModuleEvent module_event = ProtoConverter::CreateModuleUnloadedEvent(modules);

        lldbprotobuf::Event event;
        *event.mutable_module_event() = module_event;

        LOG_INFO("Broadcasting ModuleUnloaded event: " + std::to_string(modules.size()) + " modules");
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendBreakpointChangedEvent(
        const lldbprotobuf::Breakpoint &breakpoint,
        lldbprotobuf::BreakpointEventType change_type,
        const std::string &description) const {
        lldbprotobuf::BreakpointChangedEvent bp_event = ProtoConverter::CreateBreakpointChangedEvent(
            breakpoint, change_type, description);

        lldbprotobuf::Event event;
        *event.mutable_breakpoint_changed_event() = bp_event;

        LOG_INFO("Broadcasting BreakpointChanged event: breakpoint_id=" + std::to_string(breakpoint.id().id()) +
            ", change_type=" + std::to_string(change_type));
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendThreadStateChangedEvent(
        const lldbprotobuf::Thread &thread,
        lldbprotobuf::ThreadStateChangeType change_type,
        const std::string &description) const {
        lldbprotobuf::ThreadStateChangedEvent thread_event = ProtoConverter::CreateThreadStateChangedEvent(
            thread, change_type, description);

        lldbprotobuf::Event event;
        *event.mutable_thread_state_changed_event() = thread_event;

        LOG_INFO("Broadcasting ThreadStateChanged event: thread_id=" + std::to_string(thread.thread_id().id()) +
            ", change_type=" + std::to_string(change_type));
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendSymbolsLoadedEvent(
        const lldbprotobuf::Module &module,
        uint32_t symbol_count,
        const std::string &symbol_file_path) const {
        lldbprotobuf::SymbolsLoadedEvent symbols_event = ProtoConverter::CreateSymbolsLoadedEvent(
            module, symbol_count, symbol_file_path);

        lldbprotobuf::Event event;
        *event.mutable_symbols_loaded_event() = symbols_event;

        LOG_INFO("Broadcasting SymbolsLoaded event: module=" + module.name() +
            ", symbol_count=" + std::to_string(symbol_count));
        return tcp_client_.SendEventBroadcast(event);
    }


    // ============================================================================
    // Comprehensive Event Handling Implementation
    // ============================================================================

    void DebuggerClient::SetupAllEventListeners() {
        // LOG_INFO("Setting up comprehensive LLDB event listeners");
        //
        // if (!event_listener_.IsValid()) {
        //     LOG_ERROR("Event listener is not valid");
        //     return;
        // }
        //
        // // 1. 进程事件（最重要）
        // if (process_.IsValid()) {
        //     process_.GetBroadcaster().AddListener(
        //         event_listener_,
        //         lldb::SBProcess::eBroadcastBitStateChanged | // 状态改变
        //         lldb::SBProcess::eBroadcastBitInterrupt | // 中断
        //         lldb::SBProcess::eBroadcastBitSTDOUT | // 标准输出
        //         lldb::SBProcess::eBroadcastBitSTDERR | // 标准错误
        //         lldb::SBProcess::eBroadcastBitProfileData | // 性能分析
        //         lldb::SBProcess::eBroadcastBitStructuredData // 结构化数据
        //     );
        //     LOG_INFO("Registered process event listeners");
        // }
        //
        // 2. 目标事件
        if (target_.IsValid()) {
            target_.GetBroadcaster().AddListener(
                event_listener_,
                lldb::SBTarget::eBroadcastBitBreakpointChanged | // 断点改变
                lldb::SBTarget::eBroadcastBitModulesLoaded | // 模块加载
                lldb::SBTarget::eBroadcastBitModulesUnloaded | // 模块卸载
                lldb::SBTarget::eBroadcastBitWatchpointChanged | // 监视点改变
                lldb::SBTarget::eBroadcastBitSymbolsLoaded // 符号加载
            );
            LOG_INFO("Registered target event listeners");

            // 3. 断点事件通过目标广播器处理，无需单独注册
            // Breakpoint events are handled through the target broadcaster
            LOG_INFO("Breakpoint events will be handled through target broadcaster");
        }
        //
        // // 4. 线程事件通过进程广播器处理，无需单独注册
        // // Thread events are handled through the process broadcaster
        // if (process_.IsValid()) {
        //     LOG_INFO("Thread events will be handled through process broadcaster");
        // }
        //
        // // // 5. Debugger 事件（可选）
        // // if (debugger_.IsValid()) {
        // //     debugger_.GetBroadcaster().AddListener(
        // //         event_listener_,
        // //         lldb::SBDebugger::eBroadcastBitProgress |
        // //         lldb::SBDebugger::eBroadcastBitWarning |
        // //         lldb::SBDebugger::eBroadcastBitError
        // //     );
        // //     LOG_INFO("Registered debugger event listeners");
        // // }
        //
        // LOG_INFO("All LLDB event listeners setup completed");
    }

    void DebuggerClient::HandleEvent(lldb::SBEvent &event) {
        // 处理进程事件
        if (lldb::SBProcess::EventIsProcessEvent(event)) {
            HandleProcessEvent(event);
        }
        // 处理目标事件
        else if (lldb::SBTarget::EventIsTargetEvent(event)) {
            HandleTargetEvent(event);
        }
        // 处理断点事件
        else if (lldb::SBBreakpoint::EventIsBreakpointEvent(event)) {
            HandleBreakpointEvent(event);
        }
        // 处理线程事件
        else if (lldb::SBThread::EventIsThreadEvent(event)) {
            HandleThreadEvent(event);
        }

        // 其他事件
        else {
            LOG_INFO("[Other Event] " + std::string(event.GetBroadcasterClass()));
        }
    }

    void DebuggerClient::HandleProcessEvent(const lldb::SBEvent &event) {
        lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);

        LOG_INFO("[Process Event] State: " + std::string(lldb::SBDebugger::StateAsCString(state)));

        if (state == lldb::eStateStopped) {
            lldb::SBThread thread = process_.GetSelectedThread();

            switch (lldb::StopReason reason = thread.GetStopReason()) {
                case lldb::eStopReasonBreakpoint:
                    LOG_INFO("  → Breakpoint hit");
                    LogBreakpointInfo(thread);
                    break;
                case lldb::eStopReasonWatchpoint:
                    LOG_INFO("  → Watchpoint hit");
                    break;
                case lldb::eStopReasonSignal: {
                    char signal_desc[256];
                    size_t signal_len = thread.GetStopDescription(signal_desc, sizeof(signal_desc));
                    LOG_INFO("  → Signal: " + std::string(signal_desc, signal_len));
                    break;
                }
                case lldb::eStopReasonException: {
                    char exception_desc[256];
                    size_t exception_len = thread.GetStopDescription(exception_desc, sizeof(exception_desc));
                    LOG_INFO("  → Exception: " + std::string(exception_desc, exception_len));
                    break;
                }
                case lldb::eStopReasonPlanComplete:
                    LOG_INFO("  → Plan completed");
                    break;
                case lldb::eStopReasonTrace:
                    LOG_INFO("  → Single step");
                    break;
                case lldb::eStopReasonThreadExiting:
                    LOG_INFO("  → Thread exiting");
                    break;
                case lldb::eStopReasonInstrumentation:
                    LOG_INFO("  → Instrumentation event");
                    break;
                case lldb::eStopReasonExec:
                    LOG_INFO("  → Process exec");
                    break;
                case lldb::eStopReasonFork:
                    LOG_INFO("  → Process fork");
                    break;
                case lldb::eStopReasonVFork:
                    LOG_INFO("  → Process vfork");
                    break;
                case lldb::eStopReasonVForkDone:
                    LOG_INFO("  → Process vfork done");
                    break;
                default:
                    LOG_INFO("  → Other reason: " + std::to_string(reason));
                    break;
            }

            // 发送 ProcessStopped 事件，通知前端进程已停止
            // 现在使用新的详细 ThreadStopInfo 结构
            SendProcessStoppedEvent(thread);
        }

        // 处理标准输出
        if (event.GetType() & lldb::SBProcess::eBroadcastBitSTDOUT) {
            char buffer[1024];
            size_t num_bytes;
            while ((num_bytes = process_.GetSTDOUT(buffer, sizeof(buffer))) > 0) {
                std::string output_text(buffer, num_bytes);
                LOG_INFO("[STDOUT] " + output_text);

                // 发送 ProcessOutput 事件
                SendProcessOutputEvent(output_text, lldbprotobuf::OutputTypeStdout);
            }
        }

        // 处理标准错误
        if (event.GetType() & lldb::SBProcess::eBroadcastBitSTDERR) {
            char buffer[1024];
            size_t num_bytes;
            while ((num_bytes = process_.GetSTDERR(buffer, sizeof(buffer))) > 0) {
                std::string error_text(buffer, num_bytes);
                LOG_ERROR("[STDERR] " + error_text);

                // 发送 ProcessOutput 事件
                SendProcessOutputEvent(error_text, lldbprotobuf::OutputTypeStderr);
            }
        }
    }


    void DebuggerClient::HandleTargetEvent(const lldb::SBEvent &event) {
        LOG_INFO("[Target Event]");

        lldb::SBTarget target = lldb::SBTarget::GetTargetFromEvent(event);
        if (!target.IsValid()) {
            LOG_WARNING("Invalid target in event");
            return;
        }

        const uint32_t event_type = event.GetType();

        if (event_type & lldb::SBTarget::eBroadcastBitModulesLoaded) {
            LOG_INFO("Modules loaded");
            LogLoadedModules(event);

            std::vector<lldbprotobuf::Module> modules;
            uint32_t num_modules = target.GetNumModulesFromEvent(event);
            for (uint32_t i = 0; i < num_modules; ++i) {
                lldb::SBModule sb_module = target.GetModuleAtIndexFromEvent(i, event);
                if (!sb_module.IsValid()) continue;

                lldbprotobuf::Module module;
                // LLDB15 没有 GetUUID，使用索引或路径生成唯一 ID
                *module.mutable_id() = sb_module.GetUUIDString();

                char path_buf[1024] = {0};
                sb_module.GetFileSpec().GetPath(path_buf, sizeof(path_buf));
                module.set_file_path(path_buf);


                module.set_name(sb_module.GetFileSpec().GetFilename());

                module.set_is_loaded(true);

                // 获取基址
                lldb::addr_t base_address = 0;
                if (sb_module.GetNumSections() > 0) {
                    lldb::SBSection first_section = sb_module.GetSectionAtIndex(0);
                    if (first_section.IsValid()) {
                        base_address = first_section.GetLoadAddress(target);
                        if (base_address == LLDB_INVALID_ADDRESS) base_address = 0;
                    }
                }
                module.set_base_address(base_address);


                modules.push_back(module);
            }

            SendModuleLoadedEvent(modules);
        } else if (event_type & lldb::SBTarget::eBroadcastBitModulesUnloaded) {
            LOG_INFO("Modules unloaded");

            std::vector<lldbprotobuf::Module> modules;
            uint32_t num_modules = target.GetNumModulesFromEvent(event);
            for (uint32_t i = 0; i < num_modules; ++i) {
                lldb::SBModule sb_module = target.GetModuleAtIndexFromEvent(i, event);

                if (!sb_module.IsValid()) continue;

                lldbprotobuf::Module module;

                *module.mutable_id() = sb_module.GetUUIDString();

                char path_buf[1024] = {0};
                sb_module.GetFileSpec().GetPath(path_buf, sizeof(path_buf));
                module.set_file_path(path_buf);


                module.set_name(sb_module.GetFileSpec().GetFilename());

                module.set_is_loaded(true);

                // 获取基址
                lldb::addr_t base_address = 0;
                if (sb_module.GetNumSections() > 0) {
                    lldb::SBSection first_section = sb_module.GetSectionAtIndex(0);
                    if (first_section.IsValid()) {
                        base_address = first_section.GetLoadAddress(target);
                        if (base_address == LLDB_INVALID_ADDRESS) base_address = 0;
                    }
                }
                module.set_base_address(base_address);


                modules.push_back(module);
            }

            SendModuleUnloadedEvent(modules);
        } else if (event_type & lldb::SBTarget::eBroadcastBitBreakpointChanged) {
            LOG_INFO("Breakpoints changed");
          
        } else if (event_type & lldb::SBTarget::eBroadcastBitSymbolsLoaded) {
            LOG_INFO("Symbols loaded");
            std::vector<lldbprotobuf::Module> modules;
            uint32_t num_modules = target.GetNumModulesFromEvent(event);

            for (uint32_t i = 0; i < num_modules; ++i) {
                lldb::SBModule sb_module = target.GetModuleAtIndexFromEvent(i, event);

                if (!sb_module.IsValid()) continue;

                // 假设最近加载的模块就是事件对应模块
                lldbprotobuf::Module module;
                *module.mutable_id() = sb_module.GetUUIDString();

                char path_buf[1024] = {0};
                sb_module.GetFileSpec().GetPath(path_buf, sizeof(path_buf));
                module.set_file_path(path_buf);


                module.set_name(sb_module.GetFileSpec().GetFilename());

                module.set_has_symbols(true);

                // 获取符号数量
                uint32_t symbol_count = sb_module.GetNumSymbols();
                char path_buf1[1024] = {0};
                sb_module.GetSymbolFileSpec().GetPath(path_buf1, sizeof(path_buf1));
                SendSymbolsLoadedEvent(module, symbol_count, path_buf1);
                break; // 只处理一个模块
            }
        } else {
            LOG_INFO("Unknown target event");
        }
    }

    void DebuggerClient::HandleBreakpointEvent(const lldb::SBEvent &event) {
        lldb::SBBreakpoint bp = lldb::SBBreakpoint::GetBreakpointFromEvent(event);
        lldb::BreakpointEventType event_type = lldb::SBBreakpoint::GetBreakpointEventTypeFromEvent(event);

        LOG_INFO("[Breakpoint Event] Breakpoint #" + std::to_string(bp.GetID()) + " - ");

        if (!bp.IsValid()) {
            LOG_INFO("Invalid breakpoint in event");
            return;
        }

        // 转换断点事件类型
        lldbprotobuf::BreakpointEventType proto_event_type = ConvertBreakpointEventType(event_type);

        // 创建断点对象
        int64_t breakpoint_id = bp.GetID();

        // 提取源码位置信息
        lldbprotobuf::SourceLocation source_location;
        size_t num_locations = bp.GetNumLocations();
        for (size_t i = 0; i < num_locations; ++i) {
            lldb::SBBreakpointLocation loc = bp.GetLocationAtIndex(i);
            if (loc.IsValid()) {
                lldb::SBAddress addr = loc.GetAddress();
                if (addr.IsValid()) {
                    lldb::SBLineEntry line_entry = addr.GetLineEntry();
                    if (line_entry.IsValid()) {
                        lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                        if (file_spec.IsValid()) {
                            char file_path_buffer[1024];
                            file_spec.GetPath(file_path_buffer, sizeof(file_path_buffer));
                            source_location = ProtoConverter::CreateSourceLocation(
                                file_path_buffer,
                                line_entry.GetLine()
                            );
                            break;  // 使用第一个有效的位置
                        }
                    }
                }
            }
        }

        // 如果没有找到有效的位置信息，创建默认位置
        if (source_location.file_path().empty() && source_location.line() == 0) {
            source_location = ProtoConverter::CreateSourceLocation("", 0);
        }

        // 获取断点条件
        std::string condition = bp.GetCondition() ? bp.GetCondition() : "";

        lldbprotobuf::Breakpoint proto_breakpoint = ProtoConverter::CreateBreakpoint(
            breakpoint_id,
            source_location,
            condition
        );

        std::string description;
        switch (event_type) {
            case lldb::eBreakpointEventTypeAdded:
                LOG_INFO("Added");
                description = "Breakpoint added";
                break;
            case lldb::eBreakpointEventTypeRemoved:
                LOG_INFO("Removed");
                description = "Breakpoint removed";
                break;
            case lldb::eBreakpointEventTypeLocationsAdded:
                LOG_INFO("Locations added");
                description = "Breakpoint locations added";
                break;
            case lldb::eBreakpointEventTypeLocationsRemoved:
                LOG_INFO("Locations removed");
                description = "Breakpoint locations removed";
                break;
            case lldb::eBreakpointEventTypeLocationsResolved:
                LOG_INFO("Locations resolved");
                description = "Breakpoint locations resolved";
                break;
            case lldb::eBreakpointEventTypeEnabled:
                LOG_INFO("Enabled");
                description = "Breakpoint enabled";
                break;
            case lldb::eBreakpointEventTypeDisabled:
                LOG_INFO("Disabled");
                description = "Breakpoint disabled";
                break;
            case lldb::eBreakpointEventTypeCommandChanged:
                LOG_INFO("Command changed");
                description = "Breakpoint command changed";
                break;
            case lldb::eBreakpointEventTypeConditionChanged:
                LOG_INFO("Condition changed");
                description = "Breakpoint condition changed";
                break;
            default:
                LOG_INFO("Unknown event type");
                description = "Unknown breakpoint event";
                proto_event_type = lldbprotobuf::BREAKPOINT_EVENT_TYPE_UNKNOWN;
                break;
        }

        // 发送断点变化事件到前端
        SendBreakpointChangedEvent(proto_breakpoint, proto_event_type, description);
    }

    void DebuggerClient::HandleThreadEvent(const lldb::SBEvent &event) {
        LOG_INFO("[Thread Event]");

        // 从事件中获取线程信息
        lldb::SBThread thread = lldb::SBThread::GetThreadFromEvent(event);
        if (!thread.IsValid()) {
            LOG_WARNING("Invalid thread in event");
            return;
        }

        uint32_t event_type = event.GetType();
        std::string description;
        lldbprotobuf::ThreadStateChangeType change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_UNKNOWN;

        // 根据事件类型确定变化类型和描述
        if (event_type & lldb::SBThread::eBroadcastBitStackChanged) {
            LOG_INFO("Stack changed for thread " + std::to_string(thread.GetThreadID()));
            change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_STACK_CHANGED;
            description = "Thread stack changed";
        }
        if (event_type & lldb::SBThread::eBroadcastBitThreadSuspended) {
            LOG_INFO("Thread " + std::to_string(thread.GetThreadID()) + " suspended");
            change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_THREAD_SUSPENDED;
            description = "Thread suspended";
        }
        if (event_type & lldb::SBThread::eBroadcastBitThreadResumed) {
            LOG_INFO("Thread " + std::to_string(thread.GetThreadID()) + " resumed");
            change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_THREAD_RESUMED;
            description = "Thread resumed";
        }
        if (event_type & lldb::SBThread::eBroadcastBitSelectedFrameChanged) {
            LOG_INFO("Selected frame changed for thread " + std::to_string(thread.GetThreadID()));
            change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_SELECTED_FRAME_CHANGED;
            description = "Selected frame changed";
        }
        if (event_type & lldb::SBThread::eBroadcastBitThreadSelected) {
            LOG_INFO("Thread " + std::to_string(thread.GetThreadID()) + " selected");
            change_type = lldbprotobuf::THREAD_STATE_CHANGE_TYPE_THREAD_SELECTED;
            description = "Thread selected";
        }

        // 如果检测到了有效的变化类型，发送线程状态变化事件
        if (change_type != lldbprotobuf::THREAD_STATE_CHANGE_TYPE_UNKNOWN) {
            try {
                // 创建线程对象
                lldbprotobuf::Thread proto_thread = ProtoConverter::CreateThread(thread);

                // 发送线程状态变化事件到前端
                SendThreadStateChangedEvent(proto_thread, change_type, description);
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to send thread state changed event: " + std::string(e.what()));
            }
        } else {
            LOG_WARNING("Unknown thread event type: " + std::to_string(event_type));
        }
    }

    // ============================================================================
    // Helper Functions
    // ============================================================================

    lldbprotobuf::BreakpointEventType DebuggerClient::ConvertBreakpointEventType(lldb::BreakpointEventType lldb_type) const {
        switch (lldb_type) {
            case lldb::eBreakpointEventTypeAdded:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_ADDED;
            case lldb::eBreakpointEventTypeRemoved:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_REMOVED;
            case lldb::eBreakpointEventTypeLocationsAdded:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_LOCATIONS_ADDED;
            case lldb::eBreakpointEventTypeLocationsRemoved:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_LOCATIONS_REMOVED;
            case lldb::eBreakpointEventTypeLocationsResolved:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_LOCATIONS_RESOLVED;
            case lldb::eBreakpointEventTypeEnabled:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_ENABLED;
            case lldb::eBreakpointEventTypeDisabled:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_DISABLED;
            case lldb::eBreakpointEventTypeCommandChanged:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_COMMAND_CHANGED;
            case lldb::eBreakpointEventTypeConditionChanged:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_CONDITION_CHANGED;
            default:
                return lldbprotobuf::BREAKPOINT_EVENT_TYPE_UNKNOWN;
        }
    }

    void DebuggerClient::LogBreakpointInfo(lldb::SBThread &thread) const {
        uint64_t bp_id = thread.GetStopReasonDataAtIndex(0);
        uint64_t loc_id = thread.GetStopReasonDataAtIndex(1);

        LOG_INFO("  Breakpoint ID: " + std::to_string(bp_id) + ", Location ID: " + std::to_string(loc_id));

        lldb::SBFrame frame = thread.GetFrameAtIndex(0);
        lldb::SBLineEntry line_entry = frame.GetLineEntry();

        if (line_entry.IsValid()) {
            LOG_INFO("  Location: " + std::string(line_entry.GetFileSpec().GetFilename()) +
                ":" + std::to_string(line_entry.GetLine()));
        }

        LOG_INFO("  Function: " + std::string(frame.GetFunctionName() ? frame.GetFunctionName() : ""));
    }

    void DebuggerClient::LogLoadedModules(const lldb::SBEvent &event) {
        lldb::SBTarget target = lldb::SBTarget::GetTargetFromEvent(event);
        uint32_t num_modules = target.GetNumModules();

        for (uint32_t i = 0; i < num_modules; ++i) {
            lldb::SBModule module = target.GetModuleAtIndex(i);
            LOG_INFO("  Module: " + std::string(module.GetFileSpec().GetFilename()));
        }
    }

    bool DebuggerClient::HandleGetValueRequest(const lldbprotobuf::GetValueRequest &req,
                                               const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling GetValue request: variable_id=" + std::to_string(req.variable_id().id()) +
            ", max_string_length=" + std::to_string(req.max_string_length()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendGetValueResponse(false, empty_value, empty_variable, "No valid process available", hash);
        }

        // 使用新的变量映射系统查找变量
        lldb::SBValue sb_value = FindVariableById(req.variable_id().id());

        if (!sb_value.IsValid()) {
            LOG_WARNING("Variable not found or invalid with ID: " + std::to_string(req.variable_id().id()));
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendGetValueResponse(false, empty_value, empty_variable, "Variable not found or invalid", hash);
        }


        try {
            // 创建变量信息
            lldbprotobuf::Variable variable = ProtoConverter::CreateVariable(sb_value, req.variable_id().id());

            // 创建值信息
            lldbprotobuf::Value value = ProtoConverter::CreateValue(
                sb_value,
                req.variable_id().id(),
                req.max_string_length());

            LOG_INFO("Successfully created value for variable: " + variable.name() +
                " (type: " + variable.type().type_name() + ")");

            // 发送响应
            return SendGetValueResponse(true, value, variable, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to create value for variable ID " + std::to_string(req.variable_id().id()) +
                ": " + std::string(e.what()));
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendGetValueResponse(false, empty_value, empty_variable, e.what(), hash);
        }
    }

    bool DebuggerClient::HandleSetVariableValueRequest(const lldbprotobuf::SetVariableValueRequest &req,
                                                       const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling SetVariableValue request: variable_id=" + std::to_string(req.variable_id().id()) +
            ", value=" + req.value());

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendSetVariableValueResponse(false, empty_value, empty_variable, "No valid process available", hash);
        }

        // 使用变量映射系统查找变量
        lldb::SBValue sb_value = FindVariableById(req.variable_id().id());

        if (!sb_value.IsValid()) {
            LOG_WARNING("Variable not found or invalid with ID: " + std::to_string(req.variable_id().id()));
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendSetVariableValueResponse(false, empty_value, empty_variable, "Variable not found or invalid",
                                                hash);
        }

        try {
            // 使用 LLDB 的 SetValueFromCString 方法设置变量值
            lldb::SBError error;
            bool success = sb_value.SetValueFromCString(req.value().c_str(), error);

            if (error.Fail()) {
                std::string error_msg = error.GetCString() ? error.GetCString() : "Unknown error";
                LOG_ERROR("Failed to set variable value: " + error_msg);
                lldbprotobuf::Value empty_value;
                lldbprotobuf::Variable empty_variable;
                return SendSetVariableValueResponse(false, empty_value, empty_variable,
                                                    "Failed to set variable value: " + error_msg, hash);
            }

            if (!success) {
                LOG_ERROR("SetValueFromCString returned false");
                lldbprotobuf::Value empty_value;
                lldbprotobuf::Variable empty_variable;
                return SendSetVariableValueResponse(false, empty_value, empty_variable,
                                                    "SetValueFromCString returned false", hash);
            }

            // 创建变量信息
            lldbprotobuf::Variable variable = ProtoConverter::CreateVariable(sb_value,
                                                                             req.variable_id().id());

            // 创建值信息
            lldbprotobuf::Value value = ProtoConverter::CreateValue(
                sb_value,
                req.variable_id().id(),
                1024); // 使用合理的默认字符串长度

            LOG_INFO("Successfully set value for variable: " + variable.name() +
                " (type: " + variable.type().type_name() + ") to: " + req.value());

            // 发送响应
            return SendSetVariableValueResponse(true, value, variable, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to set variable value for ID " + std::to_string(req.variable_id().id()) +
                ": " + std::string(e.what()));
            lldbprotobuf::Value empty_value;
            lldbprotobuf::Variable empty_variable;
            return SendSetVariableValueResponse(false, empty_value, empty_variable, e.what(), hash);
        }
    }

    bool DebuggerClient::HandleVariablesChildrenRequest(const lldbprotobuf::VariablesChildrenRequest &req,
                                                        const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling VariablesChildren request: variable_id=" + std::to_string(req.variable_id().id()) +
            ", offset=" + std::to_string(req.offset()) +
            ", count=" + std::to_string(req.count()) +
            ", max_depth=" + std::to_string(req.max_depth()) +
            ", max_children=" + std::to_string(req.max_children()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendVariablesChildrenResponse(false, {}, 0, req.offset(), false, "No valid process available", hash);
        }

        // 使用变量映射系统查找父变量
        lldb::SBValue parent_value = FindVariableById(req.variable_id().id());

        if (!parent_value.IsValid()) {
            LOG_WARNING("Parent variable not found or invalid with ID: " + std::to_string(req.variable_id().id()));
            return SendVariablesChildrenResponse(false, {}, 0, req.offset(), false,
                                                 "Parent variable not found or invalid", hash);
        }

        // 获取变量的线程上下文信息（用于子变量ID分配）
        uint64_t thread_id = 0;
        uint32_t frame_index = 0;

        lldb::SBFrame frame = parent_value.GetFrame();
        if (frame.IsValid()) {
            lldb::SBThread thread = frame.GetThread();
            if (thread.IsValid()) {
                thread_id = thread.GetThreadID();
                // 找到该线程中包含此变量的帧索引
                uint32_t num_frames = thread.GetNumFrames();
                for (uint32_t i = 0; i < num_frames; ++i) {
                    lldb::SBFrame check_frame = thread.GetFrameAtIndex(i);
                    if (check_frame.GetFrameID() == frame.GetFrameID()) {
                        frame_index = i;
                        break;
                    }
                }
            }
        }

        std::vector<lldbprotobuf::Variable> children;
        uint32_t total_children = parent_value.GetNumChildren();

        // 计算请求的范围
        uint32_t start_idx = std::min(req.offset(), total_children);
        uint32_t end_idx = std::min(start_idx + req.count(), total_children);
        bool has_more = (end_idx < total_children);

        LOG_INFO("Variable has " + std::to_string(total_children) + " children, returning " +
            std::to_string(end_idx - start_idx) + " from index " + std::to_string(start_idx) +
            " (thread_id=" + std::to_string(thread_id) + ", frame_index=" + std::to_string(frame_index) + ")");

        // 获取指定范围的子变量
        for (uint32_t i = start_idx; i < end_idx; ++i) {
            lldb::SBValue child_value = parent_value.GetChildAtIndex(i);
            if (!child_value.IsValid()) {
                continue;
            }

            try {
                uint64_t child_id = AllocateVariableId(thread_id, frame_index, child_value);

                // 创建子变量信息
                lldbprotobuf::Variable child_variable = ProtoConverter::CreateVariable(child_value, child_id);
                children.push_back(child_variable);

                LOG_INFO("  Child: " + std::string(child_value.GetName() ? child_value.GetName() : "unnamed") +
                    " (" + std::string(child_value.GetTypeName() ? child_value.GetTypeName() : "unknown") + ")");
            } catch (const std::exception &e) {
                LOG_WARNING("Failed to convert child variable at index " + std::to_string(i) + ": " + e.what());
                // 继续处理其他子变量，不要因为一个失败而中断
            }
        }

        LOG_INFO("Successfully retrieved " + std::to_string(children.size()) + " child variables");
        return SendVariablesChildrenResponse(true, children, total_children, start_idx, has_more, "", hash);
    }

    bool DebuggerClient::SendEvaluateResponse(bool success, const lldbprotobuf::Variable &variable,
                                              const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto evaluate_resp = ProtoConverter::CreateEvaluateResponse(success, variable, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_evaluate() = evaluate_resp;

        LOG_INFO("Sending Evaluate response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
    }

    lldbprotobuf::HashId DebuggerClient::CreateHashId(unsigned long long value) const {
        lldbprotobuf::HashId hash;

        hash.set_hash(value);
        return hash;
    }

    bool DebuggerClient::SendReadMemoryResponse(bool success, uint64_t address, const std::string &data,
                                                const std::string &error_message,
                                                const std::optional<uint64_t> hash) const {
        auto read_memory_resp = ProtoConverter::CreateReadMemoryResponse(success, data, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }


        *response.mutable_read_memory() = read_memory_resp;

        LOG_INFO(
            "Sending ReadMemory response: success=" + std::to_string(success) + ", address=0x" + std::to_string(address
            ));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::HandleEvaluateRequest(const lldbprotobuf::EvaluateRequest &req,
                                               const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Evaluate request: expression='" + req.expression() + "'" +
            ", thread_id=" + std::to_string(req.thread_id().id()) +
            ", frame_index=" + std::to_string(req.frame_index()) +
            ", disable_summaries=" + std::to_string(req.disable_summaries()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for expression evaluation");
            lldbprotobuf::Variable empty_value;
            return SendEvaluateResponse(false, empty_value, "No valid process available", hash);
        }

        // 查找指定的线程
        lldb::SBThread target_thread;
        if (req.has_thread_id()) {
            uint32_t num_threads = process_.GetNumThreads();
            bool thread_found = false;

            for (uint32_t i = 0; i < num_threads; ++i) {
                lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
                if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                    target_thread = sb_thread;
                    thread_found = true;
                    break;
                }
            }

            if (!thread_found) {
                LOG_ERROR("Thread not found for expression evaluation: " + std::to_string(req.thread_id().id()));
                lldbprotobuf::Variable empty_value;
                return SendEvaluateResponse(false, empty_value, "Thread not found", hash);
            }
        } else {
            // 如果没有指定线程，使用当前选中的线程
            target_thread = process_.GetSelectedThread();
            if (!target_thread.IsValid()) {
                LOG_ERROR("No selected thread available for expression evaluation");
                lldbprotobuf::Variable empty_value;
                return SendEvaluateResponse(false, empty_value, "No selected thread available", hash);
            }
        }

        // 获取栈帧
        lldb::SBFrame target_frame;
        // frame_index is optional in protobuf, check if it's set to a non-default value
        if (req.frame_index() != 0) {
            uint32_t num_frames = target_thread.GetNumFrames();
            if (req.frame_index() >= num_frames) {
                LOG_ERROR(
                    "Frame index out of range: " + std::to_string(req.frame_index()) + " >= " + std::to_string(
                        num_frames));
                lldbprotobuf::Variable empty_value;
                return SendEvaluateResponse(false, empty_value, "Frame index out of range", hash);
            }

            target_frame = target_thread.GetFrameAtIndex(req.frame_index());
        } else {
            // 如果没有指定帧，使用当前帧（栈顶）
            target_frame = target_thread.GetFrameAtIndex(0);
        }

        if (!target_frame.IsValid()) {
            LOG_ERROR("Invalid frame for expression evaluation");
            lldbprotobuf::Variable empty_value;
            return SendEvaluateResponse(false, empty_value, "Invalid frame", hash);
        }

        try {
            // 配置表达式求值选项
            lldb::SBExpressionOptions options;

            // 根据请求配置选项
            if (req.disable_summaries()) {
                // 禁用摘要提供者，获取原始值
                // TODO: 设置相应选项
            }

            // 执行表达式求值
            lldb::SBError error;
            lldb::SBValue result = target_frame.EvaluateExpression(req.expression().c_str(), options);
            uint64_t variable_id = AllocateVariableId(
                target_thread.GetThreadID(),
                target_frame.GetFrameID(),
                result);
            lldbprotobuf::Variable variable = ProtoConverter::CreateVariable(result, variable_id);
            if (error.Fail()) {
                std::string error_msg = error.GetCString() ? error.GetCString() : "Expression evaluation failed";
                LOG_ERROR("Expression evaluation failed: " + error_msg);
                lldbprotobuf::Variable empty_value;
                return SendEvaluateResponse(false, empty_value, "Expression evaluation failed: " + error_msg, hash);
            }

            if (!result.IsValid()) {
                LOG_ERROR("Expression evaluation returned invalid result");
                lldbprotobuf::Variable empty_value;
                return SendEvaluateResponse(false, empty_value, "Expression evaluation returned invalid result", hash);
            }


            LOG_INFO("Expression evaluated successfully: '" + req.expression() + "' = " +
                (result.GetValue() ? std::string(result.GetValue()) : "<no value>"));

            return SendEvaluateResponse(true, variable, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during expression evaluation: " + std::string(e.what()));
            lldbprotobuf::Variable empty_value;
            return SendEvaluateResponse(false, empty_value,
                                        "Exception during expression evaluation: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during expression evaluation");
            lldbprotobuf::Variable empty_value;
            return SendEvaluateResponse(false, empty_value, "Unknown exception during expression evaluation", hash);
        }
    }

    bool DebuggerClient::HandleReadMemoryRequest(const lldbprotobuf::ReadMemoryRequest &req,
                                                 const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling ReadMemory request: address=0x" + std::to_string(req.address()) +
            ", size=" + std::to_string(req.size()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for memory reading");
            return SendReadMemoryResponse(false, req.address(), "", "No valid process available", hash);
        }

        try {
            // 分配缓冲区来读取内存
            size_t size_to_read = static_cast<size_t>(req.size());
            if (size_to_read == 0) {
                LOG_WARNING("Request to read 0 bytes of memory");
                return SendReadMemoryResponse(true, req.address(), "", "", hash);
            }

            // 限制最大读取大小以防止内存问题
            constexpr size_t MAX_READ_SIZE = 1024 * 1024; // 1MB
            if (size_to_read > MAX_READ_SIZE) {
                LOG_ERROR(
                    "Requested read size too large: " + std::to_string(size_to_read) + " > " + std::to_string(
                        MAX_READ_SIZE));
                return SendReadMemoryResponse(false, req.address(), "", "Requested read size too large", hash);
            }

            std::vector<uint8_t> buffer(size_to_read);

            // 使用LLDB读取内存
            lldb::SBError error;
            size_t bytes_read = process_.ReadMemory(req.address(), buffer.data(), size_to_read, error);

            if (error.Fail()) {
                std::string error_msg = error.GetCString() ? error.GetCString() : "Memory read failed";
                LOG_ERROR("Failed to read memory at address 0x" + std::to_string(req.address()) + ": " + error_msg);
                return SendReadMemoryResponse(false, req.address(), "", "Memory read failed: " + error_msg, hash);
            }

            // 将读取的字节转换为字符串（base64或直接二进制）
            std::string data;
            data.assign(reinterpret_cast<const char *>(buffer.data()), bytes_read);

            LOG_INFO(
                "Successfully read " + std::to_string(bytes_read) + " bytes from address 0x" + std::to_string(req.
                    address()));

            return SendReadMemoryResponse(true, req.address(), data, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during memory read: " + std::string(e.what()));
            return SendReadMemoryResponse(false, req.address(), "",
                                          "Exception during memory read: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during memory read");
            return SendReadMemoryResponse(false, req.address(), "", "Unknown exception during memory read", hash);
        }
    }

    bool DebuggerClient::SendWriteMemoryResponse(bool success, uint32_t bytes_written, const std::string &error_message,
                                                 const std::optional<uint64_t> hash) const {
        auto write_memory_resp = ProtoConverter::CreateWriteMemoryResponse(success, bytes_written, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_write_memory() = write_memory_resp;

        LOG_INFO(
            "Sending WriteMemory response: success=" + std::to_string(success) + ", bytes_written=" + std::to_string(
                bytes_written));
        return tcp_client_.SendProtoMessage(response);
    }


    bool DebuggerClient::HandleWriteMemoryRequest(const lldbprotobuf::WriteMemoryRequest &req,
                                                  const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling WriteMemory request: address=0x" + std::to_string(req.address()) +
            ", data_size=" + std::to_string(req.data().size()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for memory writing");
            return SendWriteMemoryResponse(false, 0, "No valid process available", hash);
        }

        try {
            // 获取要写入的数据
            const std::string &data = req.data();
            if (data.empty()) {
                LOG_WARNING("Request to write 0 bytes of memory");
                return SendWriteMemoryResponse(true, 0, "", hash);
            }

            // 限制最大写入大小以防止内存问题
            const size_t MAX_WRITE_SIZE = 1024 * 1024; // 1MB
            if (data.size() > MAX_WRITE_SIZE) {
                LOG_ERROR(
                    "Requested write size too large: " + std::to_string(data.size()) + " > " + std::to_string(
                        MAX_WRITE_SIZE));
                return SendWriteMemoryResponse(false, 0, "Requested write size too large", hash);
            }

            // 使用LLDB写入内存
            lldb::SBError error;
            size_t bytes_written = process_.WriteMemory(req.address(), data.c_str(), data.size(), error);

            if (error.Fail()) {
                std::string error_msg = error.GetCString() ? error.GetCString() : "Memory write failed";
                LOG_ERROR("Failed to write memory at address 0x" + std::to_string(req.address()) + ": " + error_msg);
                return SendWriteMemoryResponse(false, 0, "Memory write failed: " + error_msg, hash);
            }

            LOG_INFO(
                "Successfully wrote " + std::to_string(bytes_written) + " bytes to address 0x" + std::to_string(req.
                    address()));

            // 检查是否所有数据都被写入
            if (bytes_written != data.size()) {
                LOG_WARNING(
                    "Partial write: " + std::to_string(bytes_written) + " out of " + std::to_string(data.size()) +
                    " bytes written");
            }

            return SendWriteMemoryResponse(true, static_cast<uint32_t>(bytes_written), "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during memory write: " + std::string(e.what()));
            return SendWriteMemoryResponse(false, 0, "Exception during memory write: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during memory write");
            return SendWriteMemoryResponse(false, 0, "Unknown exception during memory write", hash);
        }
    }

    bool DebuggerClient::HandleDisassembleRequest(const lldbprotobuf::DisassembleRequest &req,
                                                  const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling Disassemble request: start_address=0x" + std::to_string(req.start_address()) +
            ", end_address=0x" + std::to_string(req.end_address()) +
            ", count=" + std::to_string(req.count()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for disassembly");
            return SendDisassembleResponse(false, {}, 0, "No valid process available", hash);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available for disassembly");
            return SendDisassembleResponse(false, {}, 0, "No valid target available", hash);
        }

        try {
            // 计算反汇编的范围
            uint64_t start_address = req.start_address();
            uint64_t end_address = req.end_address();
            uint32_t count = req.count();

            // 如果设置了count，则计算结束地址
            if (count > 0) {
                // 估算每条指令的平均大小（默认为15字节，x86-64的最大指令长度）
                constexpr uint32_t AVG_INSTRUCTION_SIZE = 15;
                uint64_t estimated_size = static_cast<uint64_t>(count) * AVG_INSTRUCTION_SIZE;

                // 如果没有设置结束地址或者根据count计算的距离更小，则使用估算的距离
                if (end_address == 0 || (start_address + estimated_size) < end_address) {
                    end_address = start_address + estimated_size;
                }
            }

            // 验证地址范围
            if (start_address >= end_address) {
                LOG_ERROR("Invalid address range: start_address (0x" + std::to_string(start_address) +
                    ") >= end_address (0x" + std::to_string(end_address) + ")");
                return SendDisassembleResponse(false, {}, 0, "Invalid address range", hash);
            }

            // 限制反汇编范围以防止性能问题
            const uint64_t MAX_DISASSEMBLE_SIZE = 64 * 1024; // 64KB
            if ((end_address - start_address) > MAX_DISASSEMBLE_SIZE) {
                LOG_ERROR("Requested disassemble range too large: " +
                    std::to_string(end_address - start_address) + " > " + std::to_string(MAX_DISASSEMBLE_SIZE));
                return SendDisassembleResponse(false, {}, 0, "Requested disassemble range too large", hash);
            }

            // 计算要读取的内存大小
            uint64_t read_size = end_address - start_address;
            std::vector<uint8_t> memory_buffer(read_size);

            // 读取指定地址范围的内存
            lldb::SBError error;
            size_t bytes_read = process_.ReadMemory(start_address, memory_buffer.data(), read_size, error);

            if (error.Fail() || bytes_read == 0) {
                LOG_ERROR("Failed to read memory for disassembly at address 0x" +
                    std::to_string(start_address) + ": " +
                    (error.GetCString() ? error.GetCString() : "Unknown error"));
                return SendDisassembleResponse(false, {}, 0, "Failed to read memory for disassembly", hash);
            }

            // 使用LLDB进行反汇编 - 使用ReadInstructions方法从内存反汇编
            lldb::SBInstructionList instruction_list = target_.ReadInstructions(
                lldb::SBAddress(start_address, target_),
                static_cast<uint32_t>(bytes_read));

            if (!instruction_list.IsValid()) {
                LOG_ERROR("Failed to get instruction list for address range 0x" +
                    std::to_string(start_address) + " - 0x" + std::to_string(end_address));
                return SendDisassembleResponse(false, {}, 0, "Failed to get instruction list", hash);
            }

            // 转换为protobuf格式
            std::vector<lldbprotobuf::DisassembleInstruction> instructions;
            uint32_t instruction_count = instruction_list.GetSize();
            uint32_t bytes_disassembled = 0;

            // 如果指定了count，限制返回的指令数量
            uint32_t max_instructions = (count > 0) ? std::min(count, instruction_count) : instruction_count;

            for (uint32_t i = 0; i < max_instructions; ++i) {
                lldb::SBInstruction instruction = instruction_list.GetInstructionAtIndex(i);

                if (!instruction.IsValid()) {
                    continue;
                }

                lldbprotobuf::DisassembleInstruction proto_instruction;

                // 设置地址
                proto_instruction.set_address(instruction.GetAddress().GetLoadAddress(target_));

                // 设置指令大小
                uint32_t instruction_size = instruction.GetByteSize();
                proto_instruction.set_size(instruction_size);
                bytes_disassembled += instruction_size;

                // 获取机器码
                if (req.has_options() && req.options().show_machine_code()) {
                    lldb::SBData data = instruction.GetData(target_);
                    if (data.IsValid()) {
                        size_t data_size = data.GetByteSize();
                        if (data_size > 0) {
                            std::vector<uint8_t> machine_code(data_size);
                            lldb::SBError read_error;
                            size_t machine_bytes_read = data.ReadRawData(read_error, 0, machine_code.data(), data_size);

                            if (read_error.Success() && machine_bytes_read > 0) {
                                // 直接设置原始机器码二进制数据
                                proto_instruction.set_machine_code(machine_code.data(), machine_bytes_read);
                            }
                        }
                    }
                }

                // 设置汇编指令文本
                if (const char *mnemonic = instruction.GetMnemonic(target_)) {
                    std::string instruction_text = mnemonic;
                    if (const char *operands = instruction.GetOperands(target_)) {
                        if (strlen(operands) > 0) {
                            instruction_text += " ";
                            instruction_text += operands;
                        }
                    }
                    proto_instruction.set_instruction(instruction_text);
                }

                // 设置注释（如果有的话）
                if (const char *comment = instruction.GetComment(target_)) {
                    proto_instruction.set_comment(comment);
                }

                // 获取符号信息（如果请求了符号化地址）
                if (req.has_options() && req.options().symbolize_addresses()) {
                    lldb::SBSymbol symbol = instruction.GetAddress().GetSymbol();
                    if (symbol.IsValid() && symbol.GetName()) {
                        proto_instruction.set_symbol(symbol.GetName());
                    }
                }

                // 获取源代码位置信息（如果可用）
                lldb::SBLineEntry line_entry = instruction.GetAddress().GetLineEntry();
                if (line_entry.IsValid()) {
                    lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                    if (file_spec.IsValid()) {
                        char file_path[1024];
                        file_spec.GetPath(file_path, sizeof(file_path));

                        lldbprotobuf::SourceLocation source_location;
                        source_location.set_file_path(file_path);
                        source_location.set_line(line_entry.GetLine());

                        *proto_instruction.mutable_source_location() = source_location;
                    }
                }

                instructions.push_back(std::move(proto_instruction));
            }

            LOG_INFO("Successfully disassembled " + std::to_string(instructions.size()) +
                " instructions, " + std::to_string(bytes_disassembled) + " bytes");

            return SendDisassembleResponse(true, instructions, bytes_disassembled, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during disassembly: " + std::string(e.what()));
            return SendDisassembleResponse(false, {}, 0, "Exception during disassembly: " + std::string(e.what()),
                                           hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during disassembly");
            return SendDisassembleResponse(false, {}, 0, "Unknown exception during disassembly", hash);
        }
    }

    bool DebuggerClient::SendDisassembleResponse(bool success,
                                                 const std::vector<lldbprotobuf::DisassembleInstruction> &instructions,
                                                 uint32_t bytes_disassembled,
                                                 const std::string &error_message,
                                                 const std::optional<uint64_t> hash) const {
        auto disassemble_resp = ProtoConverter::CreateDisassembleResponse(
            success,
            instructions,
            bytes_disassembled,
            error_message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_disassemble() = disassemble_resp;

        LOG_INFO("Sending Disassemble response: success=" + std::to_string(success) +
            ", instructions=" + std::to_string(instructions.size()) +
            ", bytes=" + std::to_string(bytes_disassembled));

        return tcp_client_.SendProtoMessage(response);
    }

    // ============================================================================
    // Process Termination Utilities
    // ============================================================================

    bool DebuggerClient::ForceTerminateProcess() const {
        if (!process_.IsValid()) {
            LOG_INFO("Process is invalid, nothing to terminate");
            return true;
        }

        lldb::StateType state = process_.GetState();
        if (state == lldb::StateType::eStateExited || state == lldb::StateType::eStateDetached) {
            LOG_INFO("Process already terminated, state: " + std::to_string(state));
            return true;
        }

        LOG_INFO("Forcefully terminating process, current state: " + std::to_string(state));

        // 方法1: 使用 LLDB 的 Kill 方法
        lldb::SBError error = process_.Kill();

        if (error.Success()) {
            LOG_INFO("Process killed successfully via LLDB Kill()");
            return true;
        }

        LOG_WARNING("LLDB Kill() failed: " + std::string(error.GetCString() ? error.GetCString() : "Unknown error"));

        // 方法2: 尝试发送 SIGTERM (在支持的平台上)
#ifdef __linux__
        if (process_.IsValid()) {
            lldb::pid_t pid = process_.GetProcessID();
            if (pid > 0) {
                LOG_INFO("Sending SIGTERM to process PID: " + std::to_string(pid));
                int result = ::kill(pid, SIGTERM);
                if (result == 0) {
                    // 等待一小段时间让进程优雅退出
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    state = process_.GetState();
                    if (state == lldb::StateType::eStateExited) {
                        LOG_INFO("Process terminated gracefully after SIGTERM");
                        return true;
                    }
                } else {
                    LOG_WARNING("Failed to send SIGTERM to process: " + std::string(strerror(errno)));
                }
            }
        }
#endif

        // 方法3: 强制 SIGKILL
#ifdef __linux__
        if (process_.IsValid()) {
            lldb::pid_t pid = process_.GetProcessID();
            if (pid > 0) {
                LOG_INFO("Sending SIGKILL to process PID: " + std::to_string(pid));
                int result = ::kill(pid, SIGKILL);
                if (result == 0) {
                    // 等待进程终止
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    state = process_.GetState();
                    if (state == lldb::StateType::eStateExited) {
                        LOG_INFO("Process terminated via SIGKILL");
                        return true;
                    }
                } else {
                    LOG_ERROR("Failed to send SIGKILL to process: " + std::string(strerror(errno)));
                }
            }
        }
#endif

        // 方法4: Windows 终端进程
#ifdef _WIN32
        if (process_.IsValid()) {
            lldb::pid_t pid = process_.GetProcessID();
            if (pid > 0) {
                LOG_INFO("Attempting to terminate Windows process PID: " + std::to_string(pid));
                HANDLE hProcess = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
                if (hProcess != nullptr) {
                    bool terminated = ::TerminateProcess(hProcess, 1);
                    ::CloseHandle(hProcess);

                    if (terminated) {
                        // 等待进程终止
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        state = process_.GetState();
                        if (state == lldb::StateType::eStateExited) {
                            LOG_INFO("Windows process terminated successfully");
                            return true;
                        }
                    } else {
                        LOG_ERROR("Failed to terminate Windows process: " + std::to_string(::GetLastError()));
                    }
                } else {
                    LOG_ERROR("Failed to open Windows process for termination: " + std::to_string(::GetLastError()));
                }
            }
        }
#endif

        LOG_ERROR("All process termination methods failed");
        return false;
    }

    bool DebuggerClient::WaitForProcessTermination(int timeout_ms) {
        if (!process_.IsValid()) {
            LOG_INFO("Process is invalid, already terminated");
            return true;
        }

        // 如果进程已经退出，直接返回
        lldb::StateType state = process_.GetState();
        if (state == lldb::StateType::eStateExited || state == lldb::StateType::eStateDetached) {
            LOG_INFO("Process already exited or detached, state: " + std::to_string(state));
            return true;
        }

        LOG_INFO("Waiting for process termination, timeout: " + std::to_string(timeout_ms) + "ms");

        const auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(timeout_ms);
        const auto check_interval = std::chrono::milliseconds(50); // 更频繁的检查

        while (true) {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= timeout) {
                LOG_ERROR("Process termination timeout after " + std::to_string(timeout_ms) + "ms");
                return false;
            }

            state = process_.GetState();
            if (state == lldb::StateType::eStateExited || state == lldb::StateType::eStateDetached) {
                LOG_INFO("Process terminated successfully, state: " + std::to_string(state));
                return true;
            }

            // 短暂等待避免忙等待
            std::this_thread::sleep_for(check_interval);

            // 尝试轮询LLDB事件以更新状态
            lldb::SBEvent event;
            if (event_listener_.WaitForEvent(std::chrono::milliseconds(10).count(), event)) {
                HandleEvent(event);
            }
        }
    }

    void DebuggerClient::EnsureProcessTerminated() {
        LOG_INFO("Ensuring process termination");

        if (!process_.IsValid()) {
            LOG_INFO("No valid process to terminate");
            return;
        }

        lldb::StateType state = process_.GetState();
        LOG_INFO("Current process state: " + std::to_string(state));

        // 第一步: 检查进程是否已经退出或分离
        if (state == lldb::StateType::eStateExited || state == lldb::StateType::eStateDetached) {
            LOG_INFO("Process already terminated or detached");
            return;
        }

        // 第二步: 尝试优雅终止
        LOG_INFO("Attempting graceful process termination");

        // 先尝试停止进程
        if (state != lldb::StateType::eStateStopped) {
            LOG_INFO("Stopping process before termination");
            process_.Stop();
            // 等待停止完成
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 第三步: 尝试使用 LLDB 的 Destroy 方法
        LOG_INFO("Attempting to destroy process via LLDB");
        lldb::SBError destroy_error = process_.Destroy();

        if (destroy_error.Success()) {
            LOG_INFO("Process destroyed successfully via LLDB");
            // 等待销毁完成
            if (WaitForProcessTermination(2000)) {
                return;
            }
        } else {
            LOG_WARNING("LLDB Destroy failed");
        }

        // 第四步: 强制终止
        LOG_INFO("Proceeding with forceful termination");
        if (ForceTerminateProcess()) {
            // 等待强制终止完成
            if (WaitForProcessTermination(1000)) {
                LOG_INFO("Process force terminated successfully");
                return;
            }
        }

        // 第五步: 最后的清理
        LOG_ERROR("Failed to terminate process gracefully, performing final cleanup");

        // 分离进程作为最后手段
        if (process_.IsValid()) {
            process_.Detach();
        }

        LOG_WARNING("Process termination completed with potential issues");
    }
} // namespace Cangjie::Debugger


