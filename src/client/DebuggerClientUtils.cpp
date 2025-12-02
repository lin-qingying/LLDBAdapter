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
#else
// Unix/Linux includes
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
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
    lldbprotobuf::HashId DebuggerClient::CreateHashId(unsigned long long value) {
     lldbprotobuf::HashId hash;

     hash.set_hash(value);
     return hash;
 }



}