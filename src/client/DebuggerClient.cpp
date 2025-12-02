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

#include "cangjie/debugger/DebuggerClient.h"
#include "cangjie/debugger/ProtoConverter.h"
#include "cangjie/debugger/Logger.h"

#include <functional>
#include <memory>
#include <csignal>
#include <vector>
#include <sstream>
#include <iostream>

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
        if (request.has_run_to_cursor()) {
            return HandleRunToCursorRequest(request.run_to_cursor(), request.hash());
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
        if (request.has_execute_command()) {
            return HandleExecuteCommandRequest(request.execute_command(), request.hash());
        }
        if (request.has_command_completion()) {
            return HandleCommandCompletionRequest(request.command_completion(), request.hash());
        }


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
        if (request.has_register_groups()) {
            return HandleRegisterGroupsRequest(request.register_groups(), request.hash());
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

        if (request.has_get_function_info()) {
            return HandleGetFunctionInfoRequest(request.get_function_info(), request.hash());
        }


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

        // 立即启动事件监听线程，使其可以接收后续创建的 target 的事件
        // 这样可以确保在 CreateTarget 和 AddBreakpoint 之前，事件监听器已经准备好
        if (!event_thread_running_.load()) {
            const_cast<DebuggerClient *>(this)->StartEventThread();
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


    // ============================================================================
    // Variable ID Management Implementation
    // ============================================================================











} // namespace Cangjie::Debugger


