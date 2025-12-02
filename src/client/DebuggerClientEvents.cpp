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

namespace Cangjie::Debugger {
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
        // 处理标准输出和标准错误
        if (event.GetType() & lldb::SBProcess::eBroadcastBitSTDOUT) {
            HandleProcessStdout();
        }
        if (event.GetType() & lldb::SBProcess::eBroadcastBitSTDERR) {
            HandleProcessStderr();
        }


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
        std::string state_str = lldb::SBDebugger::StateAsCString(state);

        LOG_INFO("[Process Event] State: " + state_str);

        // 使用 switch 处理所有进程状态
        switch (state) {
            case lldb::eStateInvalid:
                LOG_WARNING("  → Invalid process state");
                SendProcessStateChangedSimple(state, "Invalid process state");
                break;

            case lldb::eStateUnloaded:
                LOG_INFO("  → Process unloaded (valid but not currently loaded)");
                SendProcessStateChangedSimple(state, "Process unloaded");
                break;

            case lldb::eStateConnected:
                LOG_INFO("  → Process connected to remote debug services");
                SendProcessStateChangedSimple(state, "Connected to remote debug services");
                break;

            case lldb::eStateAttaching:
                LOG_INFO("  → Process is attaching");
                SendProcessStateChangedRunning(state, "Attaching to process", 0);
                break;

            case lldb::eStateLaunching:
                LOG_INFO("  → Process is launching");
                SendProcessStateChangedRunning(state, "Launching process", 0);
                break;

            case lldb::eStateStopped: {
                lldb::SBThread thread = process_.GetSelectedThread();
                std::string description;

                switch (lldb::StopReason reason = thread.GetStopReason()) {
                    case lldb::eStopReasonBreakpoint:
                        LOG_INFO("  → Breakpoint hit");
                        LogBreakpointInfo(thread);
                        description = "Breakpoint hit";
                        break;
                    case lldb::eStopReasonWatchpoint:
                        LOG_INFO("  → Watchpoint hit");
                        description = "Watchpoint hit";
                        break;
                    case lldb::eStopReasonSignal: {
                        char signal_desc[256];
                        size_t signal_len = thread.GetStopDescription(signal_desc, sizeof(signal_desc));
                        description = "Signal: " + std::string(signal_desc, signal_len);
                        LOG_INFO("  → " + description);
                        break;
                    }
                    case lldb::eStopReasonException: {
                        char exception_desc[256];
                        size_t exception_len = thread.GetStopDescription(exception_desc, sizeof(exception_desc));
                        description = "Exception: " + std::string(exception_desc, exception_len);
                        LOG_INFO("  → " + description);
                        break;
                    }
                    case lldb::eStopReasonPlanComplete:
                        LOG_INFO("  → Plan completed");
                        description = "Plan completed";
                        break;
                    case lldb::eStopReasonTrace:
                        LOG_INFO("  → Single step");
                        description = "Single step completed";
                        break;
                    case lldb::eStopReasonThreadExiting:
                        LOG_INFO("  → Thread exiting");
                        description = "Thread exiting";
                        break;
                    case lldb::eStopReasonInstrumentation:
                        LOG_INFO("  → Instrumentation event");
                        description = "Instrumentation event";
                        break;
                    case lldb::eStopReasonExec:
                        LOG_INFO("  → Process exec");
                        description = "Process exec";
                        break;
                    case lldb::eStopReasonFork:
                        LOG_INFO("  → Process fork");
                        description = "Process fork";
                        break;
                    case lldb::eStopReasonVFork:
                        LOG_INFO("  → Process vfork");
                        description = "Process vfork";
                        break;
                    case lldb::eStopReasonVForkDone:
                        LOG_INFO("  → Process vfork done");
                        description = "Process vfork done";
                        break;
                    default:
                        LOG_INFO("  → Other reason: " + std::to_string(reason));
                        description = "Stopped (reason: " + std::to_string(reason) + ")";
                        break;
                }

                // 获取当前帧并发送停止事件
                lldb::SBFrame frame = thread.GetFrameAtIndex(0);
                if (frame.IsValid()) {
                    SendProcessStateChangedStopped(state, description, thread, frame);
                } else {
                    LOG_WARNING("No valid frame for stopped thread");
                    SendProcessStateChangedSimple(state, description);
                }
                break;
            }

            case lldb::eStateRunning: {
                LOG_INFO("  → Process is running");
                int64_t thread_id = 0;
                lldb::SBThread thread = process_.GetSelectedThread();
                if (thread.IsValid()) {
                    thread_id = static_cast<int64_t>(thread.GetThreadID());
                }
                SendProcessStateChangedRunning(state, "Process running", thread_id);
                break;
            }

            case lldb::eStateStepping: {
                LOG_INFO("  → Process is stepping");
                int64_t thread_id = 0;
                lldb::SBThread thread = process_.GetSelectedThread();
                if (thread.IsValid()) {
                    thread_id = static_cast<int64_t>(thread.GetThreadID());
                }
                SendProcessStateChangedRunning(state, "Process stepping", thread_id);
                break;
            }

            case lldb::eStateCrashed: {
                LOG_ERROR("  → Process crashed!");
                lldb::SBThread thread = process_.GetSelectedThread();
                if (thread.IsValid()) {
                    char crash_desc[256];
                    size_t crash_len = thread.GetStopDescription(crash_desc, sizeof(crash_desc));
                    std::string description = "Process crashed: " + std::string(crash_desc, crash_len);
                    LOG_ERROR("  → " + description);

                    lldb::SBFrame frame = thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        SendProcessStateChangedStopped(state, description, thread, frame);
                    } else {
                        SendProcessStateChangedSimple(state, description);
                    }
                } else {
                    LOG_ERROR("  → No valid thread to report crash");
                    SendProcessStateChangedSimple(state, "Process crashed (no thread info)");
                }
                break;
            }

            case lldb::eStateDetached:
                LOG_INFO("  → Process detached");
                SendProcessStateChangedExited(state, "Process detached from debugger", 0, "Detached");
                break;

            case lldb::eStateExited: {
                int exit_code = process_.GetExitStatus();
                const char *exit_desc = process_.GetExitDescription();
                std::string exit_description = exit_desc ? exit_desc : "";
                LOG_INFO("  → Process exited with code: " + std::to_string(exit_code));
                if (!exit_description.empty()) {
                    LOG_INFO("  → Exit description: " + exit_description);
                }
                SendProcessStateChangedExited(state, "Process exited", exit_code, exit_description);
                break;
            }

            case lldb::eStateSuspended: {
                LOG_INFO("  → Process suspended");
                lldb::SBThread thread = process_.GetSelectedThread();
                if (thread.IsValid()) {
                    lldb::SBFrame frame = thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        SendProcessStateChangedStopped(state, "Process suspended", thread, frame);
                    } else {
                        SendProcessStateChangedSimple(state, "Process suspended");
                    }
                } else {
                    SendProcessStateChangedSimple(state, "Process suspended");
                }
                break;
            }

            default:
                LOG_WARNING("  → Unknown process state: " + std::to_string(static_cast<int>(state)));
                SendProcessStateChangedSimple(state, "Unknown state");
                break;
        }
    }

    void DebuggerClient::HandleProcessStdout() {
        char buffer[1024];
        size_t num_bytes;
        while ((num_bytes = process_.GetSTDOUT(buffer, sizeof(buffer))) > 0) {
            std::string output_text(buffer, num_bytes);
            LOG_INFO("[STDOUT] " + output_text);
            SendProcessOutputEvent(output_text, lldbprotobuf::OutputTypeStdout);
        }
    }

    void DebuggerClient::HandleProcessStderr() {
        char buffer[1024];
        size_t num_bytes;
        while ((num_bytes = process_.GetSTDERR(buffer, sizeof(buffer))) > 0) {
            std::string error_text(buffer, num_bytes);
            LOG_ERROR("[STDERR] " + error_text);
            SendProcessOutputEvent(error_text, lldbprotobuf::OutputTypeStderr);
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
                            break; // 使用第一个有效的位置
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

    bool DebuggerClient::SendInitializedEvent(

        uint64_t capabilities) const {
        auto initialized_event = ProtoConverter::CreateInitializedEvent(

            capabilities
        );

        lldbprotobuf::Event event;
        *event.mutable_initialized() = initialized_event;


        return tcp_client_.SendEventBroadcast(event);
    }

    // ========================================================================
    // Event Broadcasting Methods Implementation - Unified ProcessStateChanged
    // ========================================================================

    bool DebuggerClient::SendProcessStateChangedStopped(
        lldb::StateType state,
        const std::string &description,
        lldb::SBThread &stopped_thread,
        lldb::SBFrame &current_frame) const {
        lldbprotobuf::ProcessStateChanged process_state_changed =
                ProtoConverter::CreateProcessStateChangedStopped(state, description, stopped_thread, current_frame);

        lldbprotobuf::Event event;
        *event.mutable_process_state_changed() = process_state_changed;

        LOG_INFO("Broadcasting ProcessStateChanged (stopped): state=" +
            std::to_string(static_cast<int>(state)) + ", description=" + description);
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendProcessStateChangedRunning(
        lldb::StateType state,
        const std::string &description,
        int64_t thread_id) const {
        lldbprotobuf::ProcessStateChanged process_state_changed =
                ProtoConverter::CreateProcessStateChangedRunning(state, description, thread_id);

        lldbprotobuf::Event event;
        *event.mutable_process_state_changed() = process_state_changed;

        LOG_INFO("Broadcasting ProcessStateChanged (running): state=" +
            std::to_string(static_cast<int>(state)) + ", thread_id=" + std::to_string(thread_id));
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendProcessStateChangedExited(
        lldb::StateType state,
        const std::string &description,
        int32_t exit_code,
        const std::string &exit_description) const {
        lldbprotobuf::ProcessStateChanged process_state_changed =
                ProtoConverter::CreateProcessStateChangedExited(state, description, exit_code, exit_description);

        lldbprotobuf::Event event;
        *event.mutable_process_state_changed() = process_state_changed;

        LOG_INFO("Broadcasting ProcessStateChanged (exited): state=" +
            std::to_string(static_cast<int>(state)) + ", exit_code=" + std::to_string(exit_code));
        return tcp_client_.SendEventBroadcast(event);
    }

    bool DebuggerClient::SendProcessStateChangedSimple(
        lldb::StateType state,
        const std::string &description) const {
        lldbprotobuf::ProcessStateChanged process_state_changed =
                ProtoConverter::CreateProcessStateChangedSimple(state, description);

        lldbprotobuf::Event event;
        *event.mutable_process_state_changed() = process_state_changed;

        LOG_INFO("Broadcasting ProcessStateChanged (simple): state=" +
            std::to_string(static_cast<int>(state)) + ", description=" + description);
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
    // Helper Functions
    // ============================================================================

    lldbprotobuf::BreakpointEventType DebuggerClient::ConvertBreakpointEventType(
        lldb::BreakpointEventType lldb_type) const {
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
}
