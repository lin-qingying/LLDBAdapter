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

            // 立即注册 Target 事件监听器，以便后续添加的断点事件能被监听
            if (event_listener_.IsValid() && target_.IsValid()) {
                target_.GetBroadcaster().AddListener(
                    event_listener_,
                    lldb::SBTarget::eBroadcastBitBreakpointChanged | // 断点改变
                    lldb::SBTarget::eBroadcastBitModulesLoaded | // 模块加载
                    lldb::SBTarget::eBroadcastBitModulesUnloaded | // 模块卸载
                    lldb::SBTarget::eBroadcastBitWatchpointChanged | // 监视点改变
                    lldb::SBTarget::eBroadcastBitSymbolsLoaded // 符号加载
                );
                LOG_INFO("Registered target event listeners immediately after target creation");
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

        lldb::SBPlatform platform = target_.GetPlatform();
        LOG_INFO("Platform: " + std::string(platform.GetName()));


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
        uint32_t launch_flags = 0 | lldb::eLaunchFlagDisableSTDIO;

        LOG_INFO("  Stop at entry: NO (no breakpoints, process will run freely)");


        // 根据 console_mode 设置启动标志
        switch (req.console_mode()) {
            case lldbprotobuf::CONSOLE_MODE_PARENT:

                LOG_INFO("  Console mode: PARENT (using LLDB I/O management)");
                break;

            case lldbprotobuf::CONSOLE_MODE_EXTERNAL:
                // 在外部终端中启动
                launch_flags |= lldb::eLaunchFlagLaunchInShell;
                LOG_INFO("  Console mode: EXTERNAL (launch in shell)");
                break;

            case lldbprotobuf::CONSOLE_MODE_PSEUDO:
                // 使用伪终端

                launch_flags |= lldb::eLaunchFlagLaunchInTTY;
                LOG_INFO("  Console mode: PSEUDO (TTY/Named Pipe for bidirectional I/O)");
                break;

            default:
                LOG_WARNING("  Console mode: UNKNOWN (" + std::to_string(req.console_mode()) + "), using PARENT");
                break;
        }


        // 可选：禁用 ASLR（地址空间布局随机化），便于调试
if (launch_info.disable_aslr()) {
    launch_flags |= lldb::eLaunchFlagDisableASLR;

}

        lldb_launch_info.SetLaunchFlags(launch_flags);

        // ============================================
        // 5. 处理管道连接
        // ============================================

        // 获取LLDB设置的I/O通道信息
        const std::string &stdin_path = launch_info.stdin_path();
        const std::string &stdout_path = launch_info.stdout_path();
        const std::string &stderr_path = launch_info.stderr_path();


        LOG_INFO("Setting up process I/O management");
        LOG_INFO("  stdin: " + stdin_path);
        LOG_INFO("  stdout: " + stdout_path);
        LOG_INFO("  stderr: " + stderr_path);

        // ============================================
        // 5.1 Windows ConPTY 配置
        // ============================================
//         bool using_conpty = false;
//         if (req.console_mode() == lldbprotobuf::CONSOLE_MODE_PSEUDO) {
// #ifdef _WIN32
//             // On Windows, create ConPTY for pseudo terminal emulation
//             LOG_INFO("Platform: Windows - Creating ConPTY for pseudo terminal");
//             if (!CreateConPty(stdin_path, stdout_path, stderr_path)) {
//                 LOG_ERROR("Failed to create ConPTY");
//                 return SendLaunchResponse(false, -1, "Failed to create ConPTY for pseudo terminal", hash);
//             }
//             LOG_INFO("ConPTY created successfully - process I/O will be managed through LLDB events");
//             using_conpty = true;
// #else
//             LOG_INFO("Platform: Unix - Using native TTY support with file redirection");
// #endif
//         }

//TODO 已尝试多种方式，仍无法实现在windows行缓冲机制，lldb版本太低原因，暂时放弃conpty支持
        // ============================================
        // 5.2 配置 I/O 重定向
        // ============================================
        // 在 Windows 上使用 ConPTY 时，不使用文件重定向
        // 而是通过 LLDB 事件系统捕获输出，然后桥接到 ConPTY
        // 在其他平台或模式下，使用传统的文件重定向
        // if (!using_conpty) {
            // 如果提供了 I/O 路径，将它们设置到 LLDB launch info 中
            // 这样 LLDB 会自动将进程的 I/O 重定向到这些路径
            if (!stdin_path.empty()) {
                if (lldb_launch_info.AddOpenFileAction(STDIN_FILENO, stdin_path.c_str(), true, false)) {
                    LOG_INFO("  LLDB stdin redirected to: " + stdin_path);
                } else {
                    LOG_INFO("  LLDB stdin redirected error: " + stdin_path);
                }
            }
            if (!stdout_path.empty()) {
                if (lldb_launch_info.AddOpenFileAction(STDOUT_FILENO, stdout_path.c_str(), false, true)) {
                    LOG_INFO("  LLDB stdout redirected to: " + stdout_path);
                } else {
                    LOG_INFO("  LLDB stdout redirected error: " + stdout_path);
                }
            }
            if (!stderr_path.empty()) {
                if (lldb_launch_info.AddOpenFileAction(STDERR_FILENO, stderr_path.c_str(), false, true)) {
                    LOG_INFO("  LLDB stderr redirected to: " + stderr_path);
                } else {
                    LOG_INFO("  LLDB stderr redirected error: " + stderr_path);
                }
            }
        // } else {
        //     LOG_INFO("  Using ConPTY mode - I/O will be captured via LLDB events and forwarded to ConPTY");
        // }


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

        // 为新启动的进程设置事件监听器，包括stdout/stderr输出
        if (process_.IsValid()) {
            process_.GetBroadcaster().AddListener(
                event_listener_,
                lldb::SBProcess::eBroadcastBitStateChanged |
                lldb::SBProcess::eBroadcastBitInterrupt |
                lldb::SBProcess::eBroadcastBitSTDOUT |
                lldb::SBProcess::eBroadcastBitSTDERR |
                lldb::SBProcess::eBroadcastBitProfileData |
                lldb::SBProcess::eBroadcastBitStructuredData
            );
            LOG_INFO("Event listeners registered for launched process (including stdout/stderr)");
        }

        // ============================================
        // 8. 发送启动成功响应
        // ============================================
        // 必须在事件监控线程启动前发送响应，避免进程快速退出时事件顺序错误
        bool response_sent = SendLaunchResponse(true, static_cast<int64_t>(pid), "", hash);


        // ============================================
        // 注意：事件监听线程已经在 InitializeLLDB() 中提前启动
        // 这样可以确保在添加断点时就能接收到断点状态变更事件

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

        // 保存进程引用,以便后续调试操作使用
        process_ = process;

        // 为附加的进程设置事件监听器，包括stdout/stderr输出
        if (process_.IsValid()) {
            process_.GetBroadcaster().AddListener(
                event_listener_,
                lldb::SBProcess::eBroadcastBitStateChanged |
                lldb::SBProcess::eBroadcastBitInterrupt |
                lldb::SBProcess::eBroadcastBitSTDOUT |
                lldb::SBProcess::eBroadcastBitSTDERR |
                lldb::SBProcess::eBroadcastBitProfileData |
                lldb::SBProcess::eBroadcastBitStructuredData
            );
            LOG_INFO("Event listeners registered for attached process (including stdout/stderr)");
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

    bool DebuggerClient::HandleRunToCursorRequest(const lldbprotobuf::RunToCursorRequest &req,
                                                  const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling RunToCursor request for thread ID: " + std::to_string(req.thread_id().id()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for run to cursor");
            return SendRunToCursorResponse(false, 0, "", "No valid process available", hash);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available for run to cursor");
            return SendRunToCursorResponse(false, 0, "", "No valid target available", hash);
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
            LOG_ERROR("Thread not found for run to cursor: " + std::to_string(req.thread_id().id()));
            return SendRunToCursorResponse(false, 0, "", "Thread not found", hash);
        }

        // 设置当前线程
        process_.SetSelectedThread(target_thread);

        try {
            // 确定使用哪种方法
            bool use_address_method = false;
            uint64_t target_address = 0;
            std::string target_file;
            uint32_t target_line = 0;

            // 检查目标类型
            if (req.has_address()) {
                // 有地址目标
                target_address = req.address();
                use_address_method = !req.force_temp_breakpoint();
                LOG_INFO("RunToCursor target: address 0x" + std::to_string(target_address) +
                    ", force_temp_breakpoint=" + std::to_string(req.force_temp_breakpoint()));
            } else if (req.has_source_location()) {
                // 有源码位置目标
                const auto &location = req.source_location();
                target_file = location.file_path();
                target_line = location.line();

                // 源码位置需要使用临时断点方法
                use_address_method = false;
                LOG_INFO("RunToCursor target: " + target_file + ":" + std::to_string(target_line));
            } else {
                LOG_ERROR("No target specified for run to cursor");
                return SendRunToCursorResponse(false, 0, "", "No target specified", hash);
            }

            // 方法1: 使用 SBThread::RunToAddress()
            if (use_address_method && target_address != 0) {
                LOG_INFO("Using RunToAddress method");

                target_thread.RunToAddress(target_address);

                LOG_INFO("RunToAddress initiated successfully to address 0x" + std::to_string(target_address));
                return SendRunToCursorResponse(true, 0, "run_to_address", "", hash);
            }

            // 方法2: 使用临时断点
            LOG_INFO("Using temporary breakpoint method");

            lldb::SBBreakpoint temp_bp;

            if (target_address != 0) {
                // 使用地址创建临时断点
                temp_bp = target_.BreakpointCreateByAddress(target_address);
                LOG_INFO("Created temporary breakpoint at address 0x" + std::to_string(target_address));
            } else if (!target_file.empty() && target_line > 0) {
                // 使用源码位置创建临时断点
                temp_bp = target_.BreakpointCreateByLocation(target_file.c_str(), target_line);
                LOG_INFO("Created temporary breakpoint at " + target_file + ":" + std::to_string(target_line));
            } else {
                LOG_ERROR("Invalid target for temporary breakpoint");
                return SendRunToCursorResponse(false, 0, "", "Invalid target for temporary breakpoint", hash);
            }

            if (!temp_bp.IsValid()) {
                LOG_ERROR("Failed to create temporary breakpoint");
                return SendRunToCursorResponse(false, 0, "", "Failed to create temporary breakpoint", hash);
            }

            // 设置为一次性断点
            temp_bp.SetOneShot(true);

            uint64_t breakpoint_id = temp_bp.GetID();
            LOG_INFO("Temporary breakpoint ID: " + std::to_string(breakpoint_id) + " (one-shot)");

            // 继续执行进程
            lldb::SBError error = process_.Continue();
            if (error.Fail()) {
                LOG_ERROR("Failed to continue process: " + std::string(error.GetCString()));
                // 清理临时断点
                target_.BreakpointDelete(breakpoint_id);
                return SendRunToCursorResponse(false, 0, "",
                                               "Failed to continue process: " + std::string(error.GetCString()), hash);
            }

            LOG_INFO("RunToCursor with temporary breakpoint initiated successfully");
            return SendRunToCursorResponse(true, breakpoint_id, "temp_breakpoint", "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("RunToCursor failed for thread " + std::to_string(req.thread_id().id()) + ": " + e.what());
            return SendRunToCursorResponse(false, 0, "",
                                           "Run to cursor failed: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("RunToCursor failed with unknown error for thread " + std::to_string(req.thread_id().id()));
            return SendRunToCursorResponse(false, 0, "", "Run to cursor operation failed", hash);
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

            // 如果请求了只显示作用域内的变量，额外检查变量是否真的在作用域内
            if (req.in_scope_only()) {
                // 检查变量是否在当前PC位置的作用域内
                // 方法1: 检查变量是否有错误（通常未初始化的变量会有错误）
                lldb::SBError var_error = sb_value.GetError();

                // 方法2: 获取变量的声明信息来验证作用域
                lldb::SBDeclaration decl = sb_value.GetDeclaration();
                if (decl.IsValid()) {
                    // 获取当前执行位置
                    lldb::SBLineEntry line_entry = target_frame.GetLineEntry();
                    if (line_entry.IsValid()) {
                        uint32_t current_line = line_entry.GetLine();
                        uint32_t var_decl_line = decl.GetLine();

                        // 检查文件是否相同
                        lldb::SBFileSpec current_file = line_entry.GetFileSpec();
                        lldb::SBFileSpec var_file = decl.GetFileSpec();

                        bool same_file = false;
                        if (current_file.IsValid() && var_file.IsValid()) {
                            char current_path[1024] = {0};
                            char var_path[1024] = {0};
                            current_file.GetPath(current_path, sizeof(current_path));
                            var_file.GetPath(var_path, sizeof(var_path));
                            same_file = (std::string(current_path) == std::string(var_path));
                        }

                        // 如果在同一文件且当前行在变量声明行之前，跳过该变量
                        if (same_file && current_line < var_decl_line) {
                            const char *var_name = sb_value.GetName();
                            LOG_INFO("  Skipping variable '" +
                                std::string(var_name ? var_name : "unnamed") +
                                "' - declared at line " + std::to_string(var_decl_line) +
                                ", current line " + std::to_string(current_line) +
                                " (not yet in scope)");
                            continue;
                        }
                    }
                }
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
            ", expand_children=" + std::to_string(req.expand_children()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendRegistersResponse(false, {}, "No valid process available", hash);
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
            return SendRegistersResponse(false, {}, "Thread not found", hash);
        }

        // 获取指定的栈帧
        uint32_t num_frames = target_thread.GetNumFrames();

        if (req.frame_index() >= num_frames) {
            LOG_ERROR(
                "Frame index out of range: " + std::to_string(req.frame_index()) + " >= " + std::to_string(num_frames));
            return SendRegistersResponse(false, {}, "Frame index out of range", hash);
        }

        lldb::SBFrame target_frame = target_thread.GetFrameAtIndex(req.frame_index());
        if (!target_frame.IsValid()) {
            LOG_ERROR("Invalid frame at index " + std::to_string(req.frame_index()));
            return SendRegistersResponse(false, {}, "Invalid frame", hash);
        }

        // 获取寄存器上下文
        lldb::SBValueList reg_vars = target_frame.GetRegisters();
        if (!reg_vars.IsValid()) {
            LOG_ERROR("Failed to get register context for frame " + std::to_string(req.frame_index()));
            return SendRegistersResponse(false, {}, "Failed to get register context", hash);
        }

        std::vector<lldbprotobuf::Register> registers;

        LOG_INFO(
            "Found " + std::to_string(reg_vars.GetSize()) + " registers in frame " + std::to_string(req.frame_index()));

        // 处理寄存器组过滤
        std::set<std::string> requested_groups;
        for (const std::string &group: req.group_names()) {
            requested_groups.insert(group);
        }

        // 处理寄存器名称过滤
        std::set<std::string> requested_names;
        for (const std::string &name: req.register_names()) {
            requested_names.insert(name);
        }

        // 转换寄存器
        for (uint32_t i = 0; i < reg_vars.GetSize(); ++i) {
            lldb::SBValue reg_value = reg_vars.GetValueAtIndex(i);
            if (!reg_value.IsValid()) {
                continue;
            }

            const char *reg_name = reg_value.GetName();
            if (!reg_name) {
                continue;
            }

            // 检查寄存器名称过滤
            if (!requested_names.empty() && requested_names.find(reg_name) == requested_names.end()) {
                continue;
            }

            // 检查寄存器组过滤
            std::string register_group = reg_value.GetValueForExpressionPath(".register-set").GetValue();
            if (register_group.empty()) {
                register_group = "general";
            }

            if (!requested_groups.empty() && requested_groups.find(register_group) == requested_groups.end()) {
                continue;
            }

            try {
                // 创建寄存器protobuf对象
                lldbprotobuf::Register proto_register = ProtoConverter::CreateRegister(reg_value);

                // 设置寄存器组信息
                proto_register.set_group_name(register_group);

                // 处理子寄存器（仅当 expand_children=true 且有子元素时填充）
                if (req.expand_children() && reg_value.GetNumChildren() > 0) {
                    // 清空子元素列表，然后重新填充
                    proto_register.clear_children();

                    // 处理子寄存器（如向量寄存器的子元素）
                    for (uint32_t child_idx = 0; child_idx < reg_value.GetNumChildren(); ++child_idx) {
                        lldb::SBValue child_reg = reg_value.GetChildAtIndex(child_idx);
                        if (child_reg.IsValid()) {
                            lldbprotobuf::Register child_proto_register = ProtoConverter::CreateRegister(child_reg);
                            child_proto_register.set_group_name(register_group);
                            proto_register.add_children()->CopyFrom(child_proto_register);
                        }
                    }
                }

                registers.push_back(proto_register);

                LOG_INFO("  Register: " + std::string(reg_name) +
                    " (group: " + register_group + ") = " +
                    std::string(reg_value.GetValue() ? reg_value.GetValue() : "<no value>"));
            } catch (const std::exception &e) {
                LOG_WARNING("Failed to convert register at index " + std::to_string(i) + ": " + e.what());
                // 继续处理其他寄存器，不要因为一个失败而中断
            }
        }

        LOG_INFO("Successfully extracted " + std::to_string(registers.size()) + " registers");
        return SendRegistersResponse(true, registers, "", hash);
    }

    bool DebuggerClient::HandleRegisterGroupsRequest(const lldbprotobuf::RegisterGroupsRequest &req,
                                                     const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling RegisterGroups request: thread_id=" +
            (req.has_thread_id() ? std::to_string(req.thread_id().id()) : "current") +
            ", frame_index=" + std::to_string(req.frame_index()));

        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available");
            return SendRegisterGroupsResponse(false, {}, "No valid process available", hash);
        }

        // 如果指定了线程ID，验证线程是否存在
        if (req.has_thread_id()) {
            bool thread_found = false;
            uint32_t num_threads = process_.GetNumThreads();

            for (uint32_t i = 0; i < num_threads; ++i) {
                lldb::SBThread sb_thread = process_.GetThreadAtIndex(i);
                if (sb_thread.IsValid() && sb_thread.GetThreadID() == req.thread_id().id()) {
                    thread_found = true;
                    break;
                }
            }

            if (!thread_found) {
                LOG_ERROR("Thread not found: " + std::to_string(req.thread_id().id()));
                return SendRegisterGroupsResponse(false, {}, "Thread not found", hash);
            }
        }

        // 获取寄存器组信息
        std::vector<lldbprotobuf::RegisterGroup> register_groups;

        // 这里我们可以从当前进程/线程/帧中获取寄存器组信息
        // 由于LLDB没有直接的方法获取所有寄存器组，我们可以使用预定义的常见寄存器组
        if (target_.IsValid()) {
            // 通用寄存器组
            lldbprotobuf::RegisterGroup general_group;
            general_group.set_name("general");
            general_group.set_register_count(16); // 估算的数量
            register_groups.push_back(general_group);

            // 浮点寄存器组
            lldbprotobuf::RegisterGroup fp_group;
            fp_group.set_name("floating_point");
            fp_group.set_register_count(16); // 估算的数量
            register_groups.push_back(fp_group);

            // 向量寄存器组
            lldbprotobuf::RegisterGroup vector_group;
            vector_group.set_name("vector");
            vector_group.set_register_count(32); // 估算的数量
            register_groups.push_back(vector_group);

            // 系统寄存器组
            lldbprotobuf::RegisterGroup system_group;
            system_group.set_name("system");
            system_group.set_register_count(8); // 估算的数量
            register_groups.push_back(system_group);
        }

        LOG_INFO("Successfully extracted " + std::to_string(register_groups.size()) + " register groups");
        return SendRegisterGroupsResponse(true, register_groups, "", hash);
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
            constexpr size_t MAX_WRITE_SIZE = 1024 * 1024; // 1MB
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
        // 验证进程是否有效
        if (!process_.IsValid()) {
            LOG_ERROR("No valid process available for disassembly");
            return SendDisassembleResponse(false, {}, 0, false, 0, "No valid process available", hash);
        }

        // 验证目标是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available for disassembly");
            return SendDisassembleResponse(false, {}, 0, false, 0, "No valid target available", hash);
        }

        try {
            // 根据不同的模式解析反汇编参数
            uint64_t start_address = 0;
            uint64_t end_address = 0;
            uint32_t count = 0;
            std::string mode_type = "unknown";

            // 检查使用的是哪种模式
            switch (req.mode_case()) {
                case lldbprotobuf::DisassembleRequest::kRange: {
                    // 模式1：地址范围（向前反汇编）
                    const auto &range = req.range();
                    start_address = range.start_address();
                    end_address = range.end_address();
                    mode_type = "range";
                    LOG_INFO("Disassemble request (range mode): start_address=0x" +
                        std::to_string(start_address) + ", end_address=0x" + std::to_string(end_address));
                    break;
                }
                case lldbprotobuf::DisassembleRequest::kCount: {
                    // 模式2：从起始地址向前反汇编指定数量的指令
                    const auto &count_mode = req.count();
                    start_address = count_mode.start_address();
                    count = count_mode.instruction_count();
                    mode_type = "count";
                    LOG_INFO("Disassemble request (count mode): start_address=0x" +
                        std::to_string(start_address) + ", count=" + std::to_string(count));
                    break;
                }
                case lldbprotobuf::DisassembleRequest::kAnchor: {
                    // 模式3：以锚点为中心，按指令数量向前向后反汇编
                    const auto &anchor = req.anchor();
                    uint64_t anchor_address = anchor.anchor_address();
                    uint32_t backward_count = anchor.backward_count();
                    uint32_t forward_count = anchor.forward_count();

                    // 估算地址范围（粗略估算）
                    constexpr uint32_t AVG_INSTRUCTION_SIZE = 8;
                    start_address = anchor_address - (backward_count * AVG_INSTRUCTION_SIZE);
                    end_address = anchor_address + (forward_count * AVG_INSTRUCTION_SIZE);
                    count = backward_count + forward_count + 1; // +1 for anchor instruction
                    mode_type = "anchor";

                    LOG_INFO("Disassemble request (anchor mode): anchor_address=0x" +
                        std::to_string(anchor_address) + ", backward_count=" + std::to_string(backward_count) +
                        ", forward_count=" + std::to_string(forward_count));
                    break;
                }
                case lldbprotobuf::DisassembleRequest::kUntilPivot: {
                    // 模式4：向后反汇编到锚点
                    const auto &until_pivot = req.until_pivot();
                    start_address = until_pivot.start_address();
                    end_address = until_pivot.pivot_address();
                    mode_type = "until_pivot";

                    LOG_INFO("Disassemble request (until_pivot mode): start_address=0x" +
                        std::to_string(start_address) + ", pivot_address=0x" + std::to_string(end_address));
                    break;
                }
                default:
                    LOG_ERROR("No disassemble mode specified in request");
                    return SendDisassembleResponse(false, {}, 0, false, 0, "No disassemble mode specified", hash);
            }

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
                return SendDisassembleResponse(false, {}, 0, false, 0, "Invalid address range", hash);
            }

            // 限制反汇编范围以防止性能问题
            constexpr uint64_t MAX_DISASSEMBLE_SIZE = 64 * 1024; // 64KB
            if ((end_address - start_address) > MAX_DISASSEMBLE_SIZE) {
                LOG_ERROR("Requested disassemble range too large: " +
                    std::to_string(end_address - start_address) + " > " + std::to_string(MAX_DISASSEMBLE_SIZE));
                return SendDisassembleResponse(false, {}, 0, false, 0, "Requested disassemble range too large", hash);
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
                return SendDisassembleResponse(false, {}, 0, false, 0, "Failed to read memory for disassembly", hash);
            }

            // 使用LLDB进行反汇编 - 使用ReadInstructions方法从内存反汇编
            lldb::SBInstructionList instruction_list = target_.ReadInstructions(
                lldb::SBAddress(start_address, target_),
                static_cast<uint32_t>(bytes_read));

            if (!instruction_list.IsValid()) {
                LOG_ERROR("Failed to get instruction list for address range 0x" +
                    std::to_string(start_address) + " - 0x" + std::to_string(end_address));
                return SendDisassembleResponse(false, {}, 0, false, 0, "Failed to get instruction list", hash);
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

                // 获取指令地址
                uint64_t inst_address = instruction.GetAddress().GetLoadAddress(target_);

                // 验证指令地址在请求的范围内
                // 注意：只有在使用范围模式（end_address != 0）时才进行此检查
                if (end_address > 0 && inst_address >= end_address) {
                    LOG_INFO("Stopping disassembly: instruction at 0x" + std::to_string(inst_address) +
                        " is beyond requested end address 0x" + std::to_string(end_address));
                    break; // 超出范围，停止遍历
                }

                // 对于向前遍历的场景，还要确保不小于起始地址
                if (inst_address < start_address) {
                    LOG_WARNING("Skipping instruction at 0x" + std::to_string(inst_address) +
                        " (before requested start address 0x" + std::to_string(start_address) + ")");
                    continue;
                }

                lldbprotobuf::DisassembleInstruction proto_instruction;

                // 设置地址
                proto_instruction.set_address(inst_address);

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

            return SendDisassembleResponse(true, instructions, bytes_disassembled, false, 0, "", hash);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during disassembly: " + std::string(e.what()));
            return SendDisassembleResponse(false, {}, 0, false, 0,
                                           "Exception during disassembly: " + std::string(e.what()),
                                           hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during disassembly");
            return SendDisassembleResponse(false, {}, 0, false, 0, "Unknown exception during disassembly", hash);
        }
    }

    bool DebuggerClient::HandleGetFunctionInfoRequest(const lldbprotobuf::GetFunctionInfoRequest &req,
                                                      const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling GetFunctionInfo request");

        // 验证 target 是否有效
        if (!target_.IsValid()) {
            LOG_ERROR("No valid target available for function info query");
            return SendGetFunctionInfoResponse(false, {}, "No valid target available", hash);
        }

        std::vector<lldbprotobuf::FunctionInfo> functions;

        // 根据查询类型处理
        switch (req.query_case()) {
            case lldbprotobuf::GetFunctionInfoRequest::kAddress: {
                // 按地址查询
                uint64_t address = req.address();
                LOG_INFO("Querying function info by address: 0x" + std::to_string(address));

                // 解析地址
                lldb::SBAddress sb_address = target_.ResolveLoadAddress(address);
                if (!sb_address.IsValid()) {
                    LOG_WARNING("Failed to resolve address: 0x" + std::to_string(address));
                    return SendGetFunctionInfoResponse(false, {}, "Failed to resolve address", hash);
                }

                // 尝试获取函数信息
                lldb::SBFunction function = sb_address.GetFunction();
                if (function.IsValid()) {
                    LOG_INFO("Found function: " + std::string(function.GetName() ? function.GetName() : "unnamed"));
                    lldbprotobuf::FunctionInfo func_info = ProtoConverter::CreateFunctionInfo(function, target_);
                    functions.push_back(func_info);
                } else {
                    // 如果没有函数信息，尝试获取符号信息
                    lldb::SBSymbol symbol = sb_address.GetSymbol();
                    if (symbol.IsValid()) {
                        LOG_INFO("Found symbol: " + std::string(symbol.GetName() ? symbol.GetName() : "unnamed"));
                        lldbprotobuf::FunctionInfo func_info = ProtoConverter::CreateFunctionInfoFromSymbol(
                            symbol, target_);
                        functions.push_back(func_info);
                    } else {
                        LOG_WARNING("No function or symbol found at address: 0x" + std::to_string(address));
                        return SendGetFunctionInfoResponse(false, {}, "No function or symbol found at address", hash);
                    }
                }
                break;
            }

            case lldbprotobuf::GetFunctionInfoRequest::kName: {
                // 按名称查询
                const std::string &name = req.name();
                LOG_INFO("Querying function info by name: " + name);

                if (name.empty()) {
                    LOG_ERROR("Empty function name provided");
                    return SendGetFunctionInfoResponse(false, {}, "Empty function name", hash);
                }

                // 使用 FindFunctions 查找函数
                lldb::SBSymbolContextList symbol_contexts = target_.FindFunctions(name.c_str());
                uint32_t num_contexts = symbol_contexts.GetSize();

                LOG_INFO("Found " + std::to_string(num_contexts) + " matches for function name: " + name);

                if (num_contexts == 0) {
                    // 尝试使用 FindSymbols 查找符号
                    lldb::SBSymbolContextList symbol_list = target_.FindSymbols(name.c_str());
                    uint32_t num_symbols = symbol_list.GetSize();

                    if (num_symbols == 0) {
                        LOG_WARNING("No function or symbol found with name: " + name);
                        return SendGetFunctionInfoResponse(false, {}, "No function found with name: " + name, hash);
                    }

                    // 处理符号结果
                    for (uint32_t i = 0; i < num_symbols; ++i) {
                        lldb::SBSymbolContext ctx = symbol_list.GetContextAtIndex(i);
                        lldb::SBSymbol symbol = ctx.GetSymbol();

                        if (symbol.IsValid()) {
                            // 检查模块过滤
                            if (!req.module_name().empty()) {
                                lldb::SBModule module = ctx.GetModule();
                                if (module.IsValid()) {
                                    const char *mod_name = module.GetFileSpec().GetFilename();
                                    if (mod_name && req.module_name() != mod_name) {
                                        continue; // 跳过不匹配的模块
                                    }
                                }
                            }

                            lldbprotobuf::FunctionInfo func_info = ProtoConverter::CreateFunctionInfoFromSymbol(
                                symbol, target_);
                            functions.push_back(func_info);
                        }
                    }
                } else {
                    // 处理函数结果
                    for (uint32_t i = 0; i < num_contexts; ++i) {
                        lldb::SBSymbolContext ctx = symbol_contexts.GetContextAtIndex(i);
                        lldb::SBFunction function = ctx.GetFunction();

                        if (function.IsValid()) {
                            // 检查模块过滤
                            if (!req.module_name().empty()) {
                                lldb::SBModule module = ctx.GetModule();
                                if (module.IsValid()) {
                                    const char *mod_name = module.GetFileSpec().GetFilename();
                                    if (mod_name && req.module_name() != mod_name) {
                                        continue; // 跳过不匹配的模块
                                    }
                                }
                            }

                            lldbprotobuf::FunctionInfo func_info =
                                    ProtoConverter::CreateFunctionInfo(function, target_);
                            functions.push_back(func_info);
                        } else {
                            // 尝试从符号获取
                            lldb::SBSymbol symbol = ctx.GetSymbol();
                            if (symbol.IsValid()) {
                                if (!req.module_name().empty()) {
                                    lldb::SBModule module = ctx.GetModule();
                                    if (module.IsValid()) {
                                        const char *mod_name = module.GetFileSpec().GetFilename();
                                        if (mod_name && req.module_name() != mod_name) {
                                            continue;
                                        }
                                    }
                                }

                                lldbprotobuf::FunctionInfo func_info = ProtoConverter::CreateFunctionInfoFromSymbol(
                                    symbol, target_);
                                functions.push_back(func_info);
                            }
                        }
                    }
                }

                if (functions.empty()) {
                    LOG_WARNING("No valid functions found matching criteria");
                    return SendGetFunctionInfoResponse(false, {}, "No valid functions found", hash);
                }
                break;
            }

            default:
                LOG_ERROR("Invalid query type in GetFunctionInfoRequest");
                return SendGetFunctionInfoResponse(false, {}, "Invalid query type", hash);
        }

        LOG_INFO("Successfully retrieved " + std::to_string(functions.size()) + " function(s)");
        return SendGetFunctionInfoResponse(true, functions, "", hash);
    }

    bool DebuggerClient::HandleExecuteCommandRequest(const lldbprotobuf::ExecuteCommandRequest &req,
                                                     const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling ExecuteCommand request: command='" + req.command() + "'" +
            ", echo_command=" + std::to_string(req.echo_command()) +
            ", async_execution=" + std::to_string(req.async_execution()));

        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB for command execution");
            return SendExecuteCommandResponse(false, "", "", 0, "LLDB not available", hash);
        }

        // 获取命令解释器
        lldb::SBCommandInterpreter interpreter = debugger_.GetCommandInterpreter();
        if (!interpreter.IsValid()) {
            LOG_ERROR("Failed to get command interpreter");
            return SendExecuteCommandResponse(false, "", "", 0, "Failed to get command interpreter", hash);
        }

        // 创建返回对象来接收命令结果
        lldb::SBCommandReturnObject result;

        // 可选：设置执行上下文（线程和栈帧）
        if (req.has_thread_id() && process_.IsValid()) {
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

            if (thread_found) {
                // 设置选中的线程
                process_.SetSelectedThread(target_thread);

                // 如果指定了栈帧索引，也设置选中的栈帧
                if (req.frame_index() > 0 && req.frame_index() < target_thread.GetNumFrames()) {
                    target_thread.SetSelectedFrame(req.frame_index());
                    LOG_INFO("Set execution context: thread_id=" + std::to_string(req.thread_id().id()) +
                        ", frame_index=" + std::to_string(req.frame_index()));
                }
            } else {
                LOG_WARNING("Thread not found: " + std::to_string(req.thread_id().id()) + ", using default context");
            }
        }

        // 执行 LLDB 命令
        LOG_INFO("Executing LLDB command: " + req.command());

        // LLDB API 统一使用 HandleCommand 方法
        // 注意: async_execution 标志在这里不影响 HandleCommand 的行为
        // HandleCommand 本身是同步的，但命令内容可能触发异步操作（如 continue、run 等）
        lldb::ReturnStatus return_status = interpreter.HandleCommand(
            req.command().c_str(),
            result,
            true // add_to_history
        );

        // 提取输出
        std::string output;
        std::string error_output;

        if (result.GetOutputSize() > 0) {
            output = result.GetOutput();
        }

        if (result.GetErrorSize() > 0) {
            error_output = result.GetError();
        }

        // 如果请求回显命令，在输出前添加命令文本
        if (req.echo_command() && !output.empty()) {
            output = "(lldb) " + req.command() + "\n" + output;
        }

        // 判断命令是否成功执行
        bool success = result.Succeeded();

        LOG_INFO("Command execution completed: success=" + std::to_string(success) +
            ", return_status=" + std::to_string(return_status) +
            ", output_size=" + std::to_string(output.size()) +
            ", error_size=" + std::to_string(error_output.size()));

        // 发送响应
        return SendExecuteCommandResponse(
            success,
            output,
            error_output,
            static_cast<int32_t>(return_status),
            success ? "" : "Command execution failed",
            hash
        );
    }

    bool DebuggerClient::HandleCommandCompletionRequest(const lldbprotobuf::CommandCompletionRequest &req,
                                                        const std::optional<uint64_t> hash) const {
        LOG_INFO("Handling CommandCompletion request: partial_command='" + req.partial_command() + "'" +
            ", cursor_position=" + std::to_string(req.cursor_position()) +
            ", max_results=" + std::to_string(req.max_results()));

        // 验证 LLDB 是否已初始化
        if (!InitializeLLDB()) {
            LOG_ERROR("Failed to initialize LLDB for command completion");
            return SendCommandCompletionResponse(false, {}, "", 0, false, "LLDB not available", hash);
        }

        // 获取命令解释器
        lldb::SBCommandInterpreter interpreter = debugger_.GetCommandInterpreter();
        if (!interpreter.IsValid()) {
            LOG_ERROR("Failed to get command interpreter");
            return SendCommandCompletionResponse(false, {}, "", 0, false, "Failed to get command interpreter", hash);
        }

        try {
            // 获取要补全的部分命令
            const std::string &partial_command = req.partial_command();
            int cursor_pos = static_cast<int>(req.cursor_position());

            // 如果游标位置无效，使用命令末尾位置
            if (cursor_pos < 0 || cursor_pos > static_cast<int>(partial_command.length())) {
                cursor_pos = static_cast<int>(partial_command.length());
            }

            // 存储补全结果的容器
            lldb::SBStringList matches;
            lldb::SBStringList descriptions;

            // 使用LLDB API获取命令补全
            // HandleCompletionWithDescriptions 返回补全结果的数量
            int num_completions = interpreter.HandleCompletionWithDescriptions(
                partial_command.c_str(),
                cursor_pos,
                0, // match_start_point (输出参数，但我们不直接使用)
                req.max_results() > 0 ? static_cast<int>(req.max_results()) : -1, // max_return_elements
                matches,
                descriptions
            );

            if (num_completions < 0) {
                LOG_WARNING("Command completion returned negative count: " + std::to_string(num_completions));
                return SendCommandCompletionResponse(false, {}, "", 0, false, "Command completion failed", hash);
            }

            LOG_INFO("LLDB returned " + std::to_string(num_completions) + " completion candidates");

            // 提取补全结果到vector
            std::vector<std::string> completions;
            size_t matches_count = matches.GetSize();

            for (size_t i = 0; i < matches_count; ++i) {
                const char *match = matches.GetStringAtIndex(i);
                if (match && strlen(match) > 0) {
                    completions.push_back(match);
                }
            }

            // 计算共同前缀
            std::string common_prefix;
            if (!completions.empty()) {
                common_prefix = completions[0];

                for (size_t i = 1; i < completions.size() && !common_prefix.empty(); ++i) {
                    size_t j = 0;
                    while (j < common_prefix.length() &&
                           j < completions[i].length() &&
                           common_prefix[j] == completions[i][j]) {
                        ++j;
                    }
                    common_prefix = common_prefix.substr(0, j);
                }
            }

            // 确定补全起始位置
            // LLDB 通常会在游标位置之前的单词边界开始补全
            uint32_t completion_start = cursor_pos;

            // 向后查找单词边界
            while (completion_start > 0 &&
                   !std::isspace(partial_command[completion_start - 1])) {
                --completion_start;
            }

            // 检查是否还有更多结果（如果设置了最大结果数）
            bool has_more = false;
            if (req.max_results() > 0 && completions.size() >= req.max_results()) {
                has_more = true;
            }

            LOG_INFO("Command completion successful: " + std::to_string(completions.size()) + " results" +
                ", common_prefix='" + common_prefix + "'" +
                ", completion_start=" + std::to_string(completion_start) +
                ", has_more=" + std::to_string(has_more));

            // 发送响应
            return SendCommandCompletionResponse(
                true,
                completions,
                common_prefix,
                completion_start,
                has_more,
                "",
                hash
            );
        } catch (const std::exception &e) {
            LOG_ERROR("Exception during command completion: " + std::string(e.what()));
            return SendCommandCompletionResponse(false, {}, "", 0, false,
                                                 "Exception during command completion: " + std::string(e.what()), hash);
        } catch (...) {
            LOG_ERROR("Unknown exception during command completion");
            return SendCommandCompletionResponse(false, {}, "", 0, false, "Unknown exception during command completion",
                                                 hash);
        }
    }
}
