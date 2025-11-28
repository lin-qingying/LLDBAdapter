 // Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/debugger/DebuggerClient.h"
#include "cangjie/debugger/ProtoConverter.h"
#include "cangjie/debugger/Logger.h"

#ifdef _WIN32
#include <windows.h>

// Forward declarations for ConPTY API (Windows 10 1809+)
// These may not be available in older MinGW distributions
#ifndef HPCON
    typedef VOID* HPCON;
#endif

// ConPTY function declarations if not available in headers
extern "C" {
    typedef HRESULT (WINAPI *PFN_CreatePseudoConsole)(
        COORD size,
        HANDLE hInput,
        HANDLE hOutput,
        DWORD dwFlags,
        HPCON* phPC
    );

    typedef void (WINAPI *PFN_ClosePseudoConsole)(HPCON hPC);
}

// Function pointers for dynamic loading
static PFN_CreatePseudoConsole s_pfnCreatePseudoConsole = nullptr;
static PFN_ClosePseudoConsole s_pfnClosePseudoConsole = nullptr;
static bool s_conpty_initialized = false;

// Initialize ConPTY function pointers
static bool InitializeConPtyAPI() {
    if (s_conpty_initialized) {
        return s_pfnCreatePseudoConsole != nullptr;
    }

    s_conpty_initialized = true;

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        return false;
    }

    // Use double cast to avoid -Werror=cast-function-type warning
    // First cast FARPROC to void*, then to target function pointer type
    void* pfnCreate = reinterpret_cast<void*>(
        GetProcAddress(hKernel32, "CreatePseudoConsole")
    );
    void* pfnClose = reinterpret_cast<void*>(
        GetProcAddress(hKernel32, "ClosePseudoConsole")
    );

    s_pfnCreatePseudoConsole = reinterpret_cast<PFN_CreatePseudoConsole>(pfnCreate);
    s_pfnClosePseudoConsole = reinterpret_cast<PFN_ClosePseudoConsole>(pfnClose);

    return s_pfnCreatePseudoConsole != nullptr && s_pfnClosePseudoConsole != nullptr;
}

#endif

namespace Cangjie::Debugger {

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
    lldbprotobuf::HashId DebuggerClient::CreateHashId(unsigned long long value) const {
     lldbprotobuf::HashId hash;

     hash.set_hash(value);
     return hash;
 }

#ifdef _WIN32
    // ============================================================================
    // Windows ConPTY Support
    // ============================================================================

    bool DebuggerClient::CreateConPty(const std::string& stdin_path,
                                     const std::string& stdout_path,
                                     const std::string& stderr_path) {
     (void)stdin_path;
     (void)stderr_path;
        LOG_INFO("Creating Windows ConPTY for pseudo terminal emulation");
        LOG_INFO("Architecture: Process -> Named Pipe -> Bridge Thread -> ConPTY -> IDE Named Pipe -> IDE");

        // Check if ConPTY API is available
        if (!InitializeConPtyAPI()) {
            LOG_ERROR("ConPTY API is not available on this system (requires Windows 10 1809+)");
            return false;
        }

        // Initialize ConPTY context
        conpty_context_ = std::make_unique<ConPtyContext>();

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        // ============================================
        // 1. 创建进程输出命名管道（被调试进程写入这里）
        // ============================================
        // 生成唯一的管道名称
        DWORD pid = GetCurrentProcessId();
        auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

        std::string stdout_pipe_name = "\\\\.\\pipe\\lldb_stdout_" + std::to_string(pid) + "_" + std::to_string(timestamp);
        std::string stderr_pipe_name = "\\\\.\\pipe\\lldb_stderr_" + std::to_string(pid) + "_" + std::to_string(timestamp);

        LOG_INFO("Creating named pipes for process output:");
        LOG_INFO("  stdout: " + stdout_pipe_name);
        LOG_INFO("  stderr: " + stderr_pipe_name);

        // 创建命名管道（服务端）- 用于读取进程输出
        HANDLE hProcessStdoutRead = CreateNamedPipeA(
            stdout_pipe_name.c_str(),
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,  // 异步读取
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,  // 最多1个实例
            4096,  // 输出缓冲区大小
            4096,  // 输入缓冲区大小
            0,     // 默认超时
            &sa
        );

        if (hProcessStdoutRead == INVALID_HANDLE_VALUE) {
            LOG_ERROR("Failed to create stdout named pipe: " + std::to_string(GetLastError()));
            return false;
        }

        // 保存进程管道句柄和路径
        conpty_context_->hProcessStdout = hProcessStdoutRead;
        conpty_context_->stdout_pipe_path = stdout_pipe_name;
        conpty_context_->hProcessStderr = hProcessStdoutRead; // 暂时共用stdout
        conpty_context_->stderr_pipe_path = stdout_pipe_name;

        LOG_INFO("Created process output named pipes");

        // ============================================
        // 2. 创建ConPTY输入管道（我们的桥接线程写入这里）
        // ============================================
        HANDLE hConPtyInRead = nullptr;
        HANDLE hConPtyInWrite = nullptr;

        if (!CreatePipe(&hConPtyInRead, &hConPtyInWrite, &sa, 0)) {
            LOG_ERROR("Failed to create ConPTY input pipe: " + std::to_string(GetLastError()));
            CloseHandle(hProcessStdoutRead);
            return false;
        }
        // ConPTY读端需要继承，写端不继承
        SetHandleInformation(hConPtyInWrite, HANDLE_FLAG_INHERIT, 0);

        // 保存ConPTY输入管道的写端（桥接线程用）
        conpty_context_->hPipeIn = hConPtyInWrite;

        LOG_INFO("Created ConPTY input pipes");

        // ============================================
        // 3. 打开或创建ConPTY输出管道（ConPTY输出到这里）
        // ============================================
        HANDLE hConPtyOutWrite = nullptr;

        if (!stdout_path.empty()) {
            LOG_INFO("Opening IDE stdout named pipe: " + stdout_path);
            hConPtyOutWrite = CreateFileA(
                stdout_path.c_str(),
                GENERIC_WRITE,
                0,
                &sa,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
            if (hConPtyOutWrite == INVALID_HANDLE_VALUE) {
                LOG_ERROR("Failed to open IDE stdout named pipe: " + std::to_string(GetLastError()));
                CloseHandle(hProcessStdoutRead);
                CloseHandle(hConPtyInRead);
                CloseHandle(hConPtyInWrite);
                return false;
            }
            LOG_INFO("Connected ConPTY output to IDE named pipe");
        } else {
            LOG_ERROR("No stdout path provided for ConPTY output");
            CloseHandle(hProcessStdoutRead);
            CloseHandle(hConPtyInRead);
            CloseHandle(hConPtyInWrite);
            return false;
        }

        // ============================================
        // 4. 创建ConPTY（连接输入和输出管道）
        // ============================================
        COORD consoleSize = {120, 30};  // Default console size
        HPCON hpc = nullptr;
        HRESULT hr = s_pfnCreatePseudoConsole(
            consoleSize,
            hConPtyInRead,      // ConPTY reads from this (our bridge thread writes to hConPtyInWrite)
            hConPtyOutWrite,    // ConPTY writes to this (IDE named pipe)
            0,                  // No special flags
            &hpc
        );

        if (FAILED(hr) || hpc == nullptr) {
            LOG_ERROR("Failed to create pseudo console: " + std::to_string(hr));
            CloseHandle(hProcessStdoutRead);
            CloseHandle(hConPtyInRead);
            CloseHandle(hConPtyInWrite);
            CloseHandle(hConPtyOutWrite);
            return false;
        }

        // Store the HPCON handle
        conpty_context_->hpc = hpc;

        // Close the handles that were transferred to ConPTY
        // ConPTY now owns hConPtyInRead and hConPtyOutWrite
        CloseHandle(hConPtyInRead);
        CloseHandle(hConPtyOutWrite);

        // Important: Named pipe path is stored in conpty_context_->stdout_pipe_path
        // LLDB will open this named pipe by path for process stdout redirection
        // The bridge thread will read from hProcessStdoutRead (server side of the named pipe)

        LOG_INFO("ConPTY created successfully!");
        LOG_INFO("  Console size: " + std::to_string(consoleSize.X) + "x" + std::to_string(consoleSize.Y));
        LOG_INFO("  Data flow: Process -> Named Pipe -> Bridge Thread -> ConPTY -> IDE Named Pipe");

        // ============================================
        // 5. 启动桥接线程
        // ============================================
        conpty_context_->bridge_thread_running = true;
        conpty_context_->bridge_thread = std::thread(&DebuggerClient::ConPtyBridgeThread, this);

        LOG_INFO("ConPTY bridge thread started");
        LOG_INFO("ConPTY setup complete - ready for process launch");

        return true;
    }

    void DebuggerClient::CleanupConPty() {
        if (!conpty_context_) {
            return;
        }

        LOG_INFO("Cleaning up ConPTY resources");

        // Stop the bridge thread
        if (conpty_context_->bridge_thread_running) {
            conpty_context_->bridge_thread_running = false;
            if (conpty_context_->bridge_thread.joinable()) {
                conpty_context_->bridge_thread.join();
            }
        }

        // Close pipe handles
        if (conpty_context_->hProcessStdout) {
            CloseHandle(reinterpret_cast<HANDLE>(conpty_context_->hProcessStdout));
            conpty_context_->hProcessStdout = nullptr;
        }
        if (conpty_context_->hProcessStderr) {
            CloseHandle(reinterpret_cast<HANDLE>(conpty_context_->hProcessStderr));
            conpty_context_->hProcessStderr = nullptr;
        }
        if (conpty_context_->hPipeIn) {
            CloseHandle(reinterpret_cast<HANDLE>(conpty_context_->hPipeIn));
            conpty_context_->hPipeIn = nullptr;
        }
        if (conpty_context_->hPipeOut) {
            CloseHandle(reinterpret_cast<HANDLE>(conpty_context_->hPipeOut));
            conpty_context_->hPipeOut = nullptr;
        }

        // Close pseudo console using dynamically loaded function
        if (conpty_context_->hpc && s_pfnClosePseudoConsole) {
            s_pfnClosePseudoConsole(static_cast<HPCON>(conpty_context_->hpc));
            conpty_context_->hpc = nullptr;
        }

        conpty_context_.reset();
        LOG_INFO("ConPTY cleanup completed");
    }

    void DebuggerClient::ConPtyBridgeThread() {
        LOG_INFO("ConPTY bridge thread started");
        LOG_INFO("Bridge thread: Reading from process stdout pipe and writing to ConPTY");

        const size_t BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        HANDLE hProcessStdout = reinterpret_cast<HANDLE>(conpty_context_->hProcessStdout);
        HANDLE hConPtyIn = reinterpret_cast<HANDLE>(conpty_context_->hPipeIn);

        while (conpty_context_ && conpty_context_->bridge_thread_running) {
            if (!hProcessStdout || !hConPtyIn) {
                LOG_ERROR("Bridge thread: Invalid pipe handles");
                break;
            }

            DWORD bytesRead = 0;
            BOOL readResult = ReadFile(hProcessStdout, buffer, BUFFER_SIZE, &bytesRead, nullptr);

            if (!readResult) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    LOG_INFO("Bridge thread: Process stdout pipe closed (process terminated)");
                    break;
                }
                LOG_WARNING("Bridge thread: ReadFile error: " + std::to_string(error));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (bytesRead > 0) {
                // Write to ConPTY input
                DWORD bytesWritten = 0;
                BOOL writeResult = WriteFile(hConPtyIn, buffer, bytesRead, &bytesWritten, nullptr);

                if (!writeResult || bytesWritten != bytesRead) {
                    DWORD error = GetLastError();
                    LOG_WARNING("Bridge thread: WriteFile to ConPTY failed: " + std::to_string(error));
                    break;
                }

                LOG_INFO("Bridge thread: Forwarded " + std::to_string(bytesWritten) + " bytes to ConPTY");
            }
        }

        LOG_INFO("ConPTY bridge thread exiting");
    }

    std::string DebuggerClient::GetProcessStdoutPipePath() const {
        if (!conpty_context_ || conpty_context_->stdout_pipe_path.empty()) {
            LOG_WARNING("GetProcessStdoutPipePath: ConPTY not initialized or no stdout pipe path available");
            return "";
        }
        LOG_INFO("GetProcessStdoutPipePath: Returning " + conpty_context_->stdout_pipe_path);
        return conpty_context_->stdout_pipe_path;
    }

    std::string DebuggerClient::GetProcessStderrPipePath() const {
        if (!conpty_context_ || conpty_context_->stderr_pipe_path.empty()) {
            LOG_WARNING("GetProcessStderrPipePath: ConPTY not initialized or no stderr pipe path available");
            return "";
        }
        LOG_INFO("GetProcessStderrPipePath: Returning " + conpty_context_->stderr_pipe_path);
        return conpty_context_->stderr_pipe_path;
    }

#endif // _WIN32

}