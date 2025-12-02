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

#include "cangjie/debugger/ProtoConverter.h"



namespace Cangjie {
    namespace Debugger {
        // ============================================================================
        // 基础类型转换
        // ============================================================================

        lldbprotobuf::EnvironmentVariable ProtoConverter::CreateEnvironmentVariable(
            const std::string &name,
            const std::string &value) {
            lldbprotobuf::EnvironmentVariable env_var;
            env_var.set_name(name);
            env_var.set_value(value);
            return env_var;
        }


        lldbprotobuf::Hash ProtoConverter::CreateHash(lldbprotobuf::HashAlgorithm hash_algorithm,
                                                      const std::string &string) {
            lldbprotobuf::Hash hash;
            hash.set_hash_algorithm(hash_algorithm);
            hash.set_hash_value(string);
            return hash;
        }

        lldbprotobuf::SourceLocation ProtoConverter::CreateSourceLocation(
            const std::string &file_path,
            uint32_t line,
            lldbprotobuf::HashAlgorithm hash_algorithm,
            const std::string &hash_value) {
            lldbprotobuf::SourceLocation location;
            location.set_file_path(file_path);
            location.set_line(line);
            *location.mutable_hash() = CreateHash(hash_algorithm, hash_value);

            return location;
        }


        // ============================================================================
        // 线程和执行状态转换
        // ============================================================================
        lldbprotobuf::StopReason ProtoConverter::CreateStopReason(
            lldb::StopReason lldb_stop_reason) {
            switch (lldb_stop_reason) {
                case lldb::eStopReasonTrace:
                    return lldbprotobuf::STOP_REASON_TRACE;
                case lldb::eStopReasonBreakpoint:
                    return lldbprotobuf::STOP_REASON_BREAKPOINT;
                case lldb::eStopReasonWatchpoint:
                    return lldbprotobuf::STOP_REASON_WATCHPOINT;
                case lldb::eStopReasonSignal:
                    return lldbprotobuf::STOP_REASON_SIGNAL;
                case lldb::eStopReasonException:
                    return lldbprotobuf::STOP_REASON_EXCEPTION;
                case lldb::eStopReasonExec:
                    return lldbprotobuf::STOP_REASON_EXEC;
                case lldb::eStopReasonPlanComplete:
                    return lldbprotobuf::STOP_REASON_PLAN_COMPLETE;
                case lldb::eStopReasonThreadExiting:
                    return lldbprotobuf::STOP_REASON_THREAD_EXITING;
                case lldb::eStopReasonInstrumentation:
                    return lldbprotobuf::STOP_REASON_INSTRUMENTATION;
                case lldb::eStopReasonFork:
                    return lldbprotobuf::STOP_REASON_FORK;
                case lldb::eStopReasonVFork:
                    return lldbprotobuf::STOP_REASON_VFORK;
                case lldb::eStopReasonVForkDone:
                    return lldbprotobuf::STOP_REASON_VFORK_DONE;
                default:
                    return lldbprotobuf::STOP_REASON_UNKNOWN;
            }
        }

        lldbprotobuf::ThreadStopInfo ProtoConverter::CreateThreadStopInfo(
            lldb::SBThread &sb_thread) {
            lldbprotobuf::ThreadStopInfo stop_info;

            // 获取停止原因
            lldb::StopReason lldb_reason = sb_thread.GetStopReason();
            lldbprotobuf::StopReason reason = CreateStopReason(lldb_reason);
            stop_info.set_reason(reason);

            // 获取停止描述
            char stop_desc[256];
            size_t desc_len = sb_thread.GetStopDescription(stop_desc, sizeof(stop_desc));
            std::string description = std::string(stop_desc, desc_len);
            stop_info.set_description(description);

            // 根据停止原因填充详细信息 - 所有值都从LLDB API获取
            switch (lldb_reason) {
                case lldb::eStopReasonBreakpoint: {
                    lldbprotobuf::BreakpointStopInfo* breakpoint_info =
                        stop_info.mutable_breakpoint_info();

                    // 从LLDB API获取断点ID
                    uint64_t bp_id = sb_thread.GetStopReasonDataAtIndex(0);

                    breakpoint_info->set_breakpoint_id(bp_id);

                    // 通过目标获取断点详细信息来确定类型
                    lldb::SBFrame frame = sb_thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        lldb::SBProcess process = frame.GetThread().GetProcess();
                        if (process.IsValid()) {
                            lldb::SBTarget target = process.GetTarget();
                            if (target.IsValid()) {
                                lldb::SBBreakpoint bp = target.FindBreakpointByID(bp_id);
                                if (bp.IsValid()) {
                                    // 根据断点类型确定类型
                                    if (bp.IsInternal()) {
                                        breakpoint_info->set_type(lldbprotobuf::BREAKPOINT_TYPE_ADDRESS);
                                    } else {
                                        // 简化断点类型判断，避免使用不完整的 SBBreakpointLocation
                                        breakpoint_info->set_type(lldbprotobuf::BREAKPOINT_TYPE_LINE);
                                    }
                                } else {
                                    breakpoint_info->set_type(lldbprotobuf::BREAKPOINT_TYPE_LINE);
                                }
                            }
                        }
                    }

                    // 获取断点地址
                    if (frame.IsValid()) {
                        breakpoint_info->set_address(frame.GetPC());
                    }

                    // 获取实际命中次数
                    if (frame.IsValid()) {
                        lldb::SBProcess process = frame.GetThread().GetProcess();
                        if (process.IsValid()) {
                            lldb::SBTarget target = process.GetTarget();
                            if (target.IsValid()) {
                                lldb::SBBreakpoint bp = target.FindBreakpointByID(bp_id);
                                if (bp.IsValid()) {
                                    uint32_t hit_count = bp.GetHitCount();
                                    breakpoint_info->set_hit_count(hit_count);
                                }
                            }
                        }
                    }

                    break;
                }

                case lldb::eStopReasonWatchpoint: {
                    lldbprotobuf::WatchpointStopInfo* watchpoint_info =
                        stop_info.mutable_watchpoint_info();

                    // 从LLDB API获取观察点信息
                    uint64_t wp_id = sb_thread.GetStopReasonDataAtIndex(0);
                    uint64_t wp_addr = sb_thread.GetStopReasonDataAtIndex(1);
                    uint32_t wp_size = static_cast<uint32_t>(sb_thread.GetStopReasonDataAtIndex(2));

                    watchpoint_info->set_watchpoint_id(wp_id);
                    watchpoint_info->set_address(wp_addr);
                    watchpoint_info->set_size(wp_size);

                    // 获取观察点类型信息
                    lldb::SBProcess process = sb_thread.GetProcess();
                    if (process.IsValid()) {
                        lldb::SBTarget target = process.GetTarget();
                        if (target.IsValid()) {
                            lldb::SBWatchpoint wp = target.FindWatchpointByID(wp_id);
                            if (wp.IsValid()) {
                               // 获取观察点类型
                                uint32_t wp_type = wp.GetWatchSize();
                                if (wp_type & 0x1) { // 写观察点
                                    watchpoint_info->set_watch_type(lldbprotobuf::WATCH_TYPE_WRITE);
                                } else if (wp_type & 0x2) { // 读观察点
                                    watchpoint_info->set_watch_type(lldbprotobuf::WATCH_TYPE_READ);
                                } else { // 读写观察点
                                    watchpoint_info->set_watch_type(lldbprotobuf::WATCH_TYPE_READ_WRITE);
                                }                            }
                        }
                    }

                    break;
                }

                case lldb::eStopReasonSignal: {
                    lldbprotobuf::SignalStopInfo* signal_info =
                        stop_info.mutable_signal_info();

                    // 从LLDB API获取信号信息
                    int32_t signal_num = static_cast<int32_t>(sb_thread.GetStopReasonDataAtIndex(0));

                    signal_info->set_signal_number(signal_num);
                    signal_info->set_signal_name(GetSignalName(signal_num));

                    break;
                }

                case lldb::eStopReasonException: {
                    lldbprotobuf::ExceptionStopInfo* exception_info =
                        stop_info.mutable_exception_stop_info();

                    // 从LLDB API获取异常信息
                    uint64_t exception_addr = sb_thread.GetStopReasonDataAtIndex(0);
                    int32_t exception_code = static_cast<int32_t>(sb_thread.GetStopReasonDataAtIndex(1));

                    exception_info->set_exception_code(exception_code);
                    exception_info->set_exception_address(exception_addr);
                    exception_info->set_message(description);

                    // 获取异常发生的源代码位置
                    lldb::SBFrame frame = sb_thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        lldb::SBLineEntry line_entry = frame.GetLineEntry();
                        if (line_entry.IsValid()) {
                            lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                            if (file_spec.IsValid()) {
                                char file_path[1024];
                                file_spec.GetPath(file_path, sizeof(file_path));
                                lldbprotobuf::SourceLocation* location = exception_info->mutable_location();
                                location->set_file_path(file_path);
                                location->set_line(line_entry.GetLine());
                            }
                        }
                    }

                    // 尝试获取异常类型信息 - 使用已有description变量
                    if (!description.empty()) {
                        std::string desc_str = description;
                        // 根据描述推断异常类型
                        if (desc_str.find("access") != std::string::npos ||
                            desc_str.find("violation") != std::string::npos) {
                            exception_info->set_exception_type("access_violation");
                            exception_info->set_exception_name("Access Violation");
                        } else if (desc_str.find("division") != std::string::npos) {
                            exception_info->set_exception_type("arithmetic_exception");
                            exception_info->set_exception_name("Division Exception");
                        } else if (desc_str.find("overflow") != std::string::npos) {
                            exception_info->set_exception_type("arithmetic_exception");
                            exception_info->set_exception_name("Overflow Exception");
                        } else {
                            exception_info->set_exception_type("runtime_exception");
                            exception_info->set_exception_name("Runtime Exception");
                        }
                    } else {
                        exception_info->set_exception_type("runtime_exception");
                        exception_info->set_exception_name("Exception");
                    }

                    break;
                }

                case lldb::eStopReasonTrace: {
                    lldbprotobuf::StepStopInfo* step_info =
                        stop_info.mutable_step_info();

                    // 获取当前源代码位置
                    lldb::SBFrame frame = sb_thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        lldb::SBLineEntry line_entry = frame.GetLineEntry();
                        if (line_entry.IsValid()) {
                            lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                            if (file_spec.IsValid()) {
                                char file_path[1024];
                                file_spec.GetPath(file_path, sizeof(file_path));
                                lldbprotobuf::SourceLocation* location = step_info->mutable_location();
                                location->set_file_path(file_path);
                                location->set_line(line_entry.GetLine());
                            }
                        }

                        // 由于 GetPreviousPC() 不存在，我们根据单步数据来判断步进范围
                        uint32_t step_data_count_for_range = sb_thread.GetStopReasonDataCount();
                        if (step_data_count_for_range > 0) {
                            // 如果有停止数据，认为是指令级单步
                            step_info->set_step_range(lldbprotobuf::STEP_RANGE_INSTRUCTION);
                        } else {
                            // 否则认为是行级单步
                            step_info->set_step_range(lldbprotobuf::STEP_RANGE_LINE);
                        }
                    }

                    // 通过线程信息判断单步类型
                    uint32_t step_data_count = sb_thread.GetStopReasonDataCount();
                    if (step_data_count > 0) {
                        uint64_t step_data = sb_thread.GetStopReasonDataAtIndex(0);
                        // 根据停止数据判断步进类型
                        if (step_data == 1) {
                            step_info->set_step_type(lldbprotobuf::STEP_TYPE_INTO);
                        } else if (step_data == 2) {
                            step_info->set_step_type(lldbprotobuf::STEP_TYPE_OVER);
                        } else if (step_data == 3) {
                            step_info->set_step_type(lldbprotobuf::STEP_TYPE_OUT);
                        } else {
                            step_info->set_step_type(lldbprotobuf::STEP_TYPE_INTO);
                        }
                    } else {
                        step_info->set_step_type(lldbprotobuf::STEP_TYPE_INTO);
                    }

                    break;
                }

                case lldb::eStopReasonPlanComplete: {
                    lldbprotobuf::PlanCompleteStopInfo* plan_info =
                        stop_info.mutable_plan_complete_info();

                    // 从LLDB获取计划完成信息
                    plan_info->set_result_description(description);

                    // 获取计划状态信息
                    lldb::SBFrame frame = sb_thread.GetFrameAtIndex(0);
                    if (frame.IsValid()) {
                        lldb::SBThread thread = frame.GetThread();
                        if (thread.IsValid()) {
                            // 根据停止描述判断计划类型和状态
                            if (description.find("step") != std::string::npos) {
                                plan_info->set_plan_type("step_plan");
                            } else if (description.find("continue") != std::string::npos) {
                                plan_info->set_plan_type("continue_plan");
                            } else if (description.find("until") != std::string::npos) {
                                plan_info->set_plan_type("until_plan");
                            } else {
                                plan_info->set_plan_type("execution_plan");
                            }

                            // 默认为成功状态
                            plan_info->set_status(lldbprotobuf::COMPLETION_STATUS_SUCCESS);
                        }
                    }

                    // 尝试获取执行的步数
                    uint32_t data_count = sb_thread.GetStopReasonDataCount();
                    if (data_count > 0) {
                        plan_info->set_steps_executed(static_cast<uint32_t>(sb_thread.GetStopReasonDataAtIndex(0)));
                    }

                    break;
                }

                case lldb::eStopReasonThreadExiting: {
                    lldbprotobuf::ThreadExitStopInfo* exit_info =
                        stop_info.mutable_thread_exit_info();

                    // 从LLDB API获取线程退出信息
                    int32_t exit_code = static_cast<int32_t>(sb_thread.GetStopReasonDataAtIndex(0));
                    exit_info->set_exit_code(exit_code);

                    // 获取线程信息
                    uint32_t index_id = sb_thread.GetIndexID();

                    // 判断是否为主线程（通常是索引为0的线程）
                    exit_info->set_is_main_thread(index_id == 0);

                    // 设置退出原因
                    if (exit_code == 0) {
                        exit_info->set_exit_reason("Thread completed successfully");
                    } else {
                        exit_info->set_exit_reason("Thread terminated with error code: " + std::to_string(exit_code));
                    }

                    break;
                }

                case lldb::eStopReasonInstrumentation: {
                    lldbprotobuf::InstrumentationStopInfo* instr_info =
                        stop_info.mutable_instrumentation_info();

                    // 从LLDB API获取工具化事件信息
                    instr_info->set_event_data(description);

                    // 获取事件ID和工具名称
                    uint32_t data_count = sb_thread.GetStopReasonDataCount();
                    if (data_count > 0) {
                        instr_info->set_event_id(static_cast<uint64_t>(sb_thread.GetStopReasonDataAtIndex(0)));
                    }

                    // 根据描述判断工具名称和事件类型
                    if (description.find("trace") != std::string::npos) {
                        instr_info->set_tool_name("trace_tool");
                        instr_info->set_event_type("trace_event");
                    } else if (description.find("profile") != std::string::npos) {
                        instr_info->set_tool_name("profiler");
                        instr_info->set_event_type("profile_event");
                    } else if (description.find("coverage") != std::string::npos) {
                        instr_info->set_tool_name("coverage_tool");
                        instr_info->set_event_type("coverage_event");
                    } else {
                        instr_info->set_tool_name("lldb");
                        instr_info->set_event_type("instrumentation_event");
                    }

                    break;
                }

                default:
                    // 对于其他停止原因，不设置详细的stop_details
                    break;
            }

            return stop_info;
        }

        // 辅助函数：获取信号名称
        std::string ProtoConverter::GetSignalName(int32_t signal_num) {
            // 常见信号名称映射
            switch (signal_num) {
                case 2: return "SIGINT";
                case 3: return "SIGQUIT";
                case 4: return "SIGILL";
                case 5: return "SIGTRAP";
                case 6: return "SIGABRT";
                case 8: return "SIGFPE";
                case 9: return "SIGKILL";
                case 10: return "SIGUSR1";
                case 11: return "SIGSEGV";
                case 12: return "SIGUSR2";
                case 13: return "SIGPIPE";
                case 14: return "SIGALRM";
                case 15: return "SIGTERM";
                case 17: return "SIGCHLD";
                case 19: return "SIGSTOP";
                case 20: return "SIGTSTP";
                case 21: return "SIGTTIN";
                case 22: return "SIGTTOU";
                default:
                    return "SIGUNKNOWN";
            }
        }

        // 辅助函数：判断是否为致命信号
        bool ProtoConverter::IsFatalSignal(int32_t signal_num) {
            // 常见致命信号列表
            switch (signal_num) {
                case 4:  // SIGILL
                case 6:  // SIGABRT
                case 8:  // SIGFPE
                case 9:  // SIGKILL
                case 11: // SIGSEGV
                case 13: // SIGPIPE
                case 15: // SIGTERM
                case 24: // SIGXCPU
                case 25: // SIGXFSZ
                    return true;
                default:
                    return false;
            }
        }

        lldbprotobuf::Thread ProtoConverter::CreateThread(lldb::SBThread &sb_thread) {
            // 创建 protobuf Thread 对象
            lldbprotobuf::Thread thread;
            thread.set_index(sb_thread.GetIndexID());
            *thread.mutable_thread_id() = CreateId(sb_thread.GetThreadID());

            thread.set_name(sb_thread.GetName() ? sb_thread.GetName() : "");
            thread.set_is_frozen(false);

            // 使用新的 CreateThreadStopInfo 方法直接从 SBThread 创建详细的停止信息
            *thread.mutable_stop_info() = CreateThreadStopInfo(sb_thread);

            return thread;
        }

        lldbprotobuf::Frame ProtoConverter::CreateFrame(lldb::SBFrame &sb_frame) {
            lldbprotobuf::Frame frame;


            frame.set_index(sb_frame.GetFrameID());
            *frame.mutable_id() = CreateId(sb_frame.GetFrameID());
            // 设置函数名
            if (const char *function_name = sb_frame.GetFunctionName()) {
                frame.set_function_name(function_name);
            }
            //设置模块名
            lldb::SBModule sb_module = sb_frame.GetModule();
            if (sb_module.IsValid()) {
                frame.set_module(sb_module.GetFileSpec().GetFilename());
            }

            // 设置程序计数器
            frame.set_program_counter(sb_frame.GetPC());

            // 创建源码位置信息
            lldbprotobuf::SourceLocation location;
            const lldb::SBLineEntry line_entry = sb_frame.GetLineEntry();
            if (line_entry.IsValid()) {
                lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                if (file_spec.IsValid()) {
                    char file_path[1024];
                    file_spec.GetPath(file_path, sizeof(file_path));
                    location = CreateSourceLocation(file_path, line_entry.GetLine());
                }
            }

            *frame.mutable_location() = location;
            return frame;
        }

        lldbprotobuf::Type ProtoConverter::CreateType(std::string type_name,
                                                      std::optional<lldbprotobuf::TypeKind> type_kind,
                                                      std::string display_type) {
            lldbprotobuf::Type type;

            // 设置类型名称
            if (!type_name.empty()) {
                type.set_type_name(type_name);
            } else {
                type.set_type_name("<unknown>");
            }

            // 设置显示类型名称
            if (!display_type.empty()) {
                type.set_display_type(display_type);
            } else {
                // 如果没有显示类型名称，尝试使用类型名称
                type.set_display_type(type.type_name());
            }

            // 设置类型种类（处理可能为空的情况）
            if (type_kind.has_value()) {
                type.set_type_kind(type_kind.value());
            }

            return type;
        }

        lldbprotobuf::Type ProtoConverter::CreateType(lldb::SBType &sb_type) {
            lldbprotobuf::Type type;

            // 设置类型名称
            if (const char *type_name = sb_type.GetName()) {
                type.set_type_name(type_name);
            } else {
                type.set_type_name("<unknown>");
            }

            // 设置显示类型名称（使用更友好的显示名称）
            if (const char *display_name = sb_type.GetDisplayTypeName()) {
                type.set_display_type(display_name);
            } else {
                // 如果没有显示类型名称，尝试使用类型名称
                type.set_display_type(type.type_name());
            }

            // 使用独立的转换方法
            lldb::TypeClass type_class = sb_type.GetTypeClass();
            type.set_type_kind(ConvertTypeKind(type_class));

            return type;
        }

        lldbprotobuf::Variable ProtoConverter::CreateVariable(lldb::SBValue &sb_value, uint64_t variable_id) {
            lldbprotobuf::Variable variable;
            variable.mutable_id()->set_id(variable_id);
            // 设置变量名称
            if (const char *name = sb_value.GetName()) {
                variable.set_name(name);
            } else {
                variable.set_name("<unnamed>");
            }


            // 安全地设置变量类型
            try {
                lldb::SBType sb_type = sb_value.GetTarget().FindFirstType(sb_value.GetTypeName());
                if (sb_type.IsValid()) {
                    *variable.mutable_type() = CreateType(sb_type);
                } else {
                    *variable.mutable_type() = CreateType(
                        sb_value.GetTypeName() ? sb_value.GetTypeName() : "",
                        std::nullopt,
                        sb_value.GetDisplayTypeName() ? sb_value.GetDisplayTypeName() : "");
                }
            } catch (...) {
                *variable.mutable_type() = CreateType(
                    sb_value.GetTypeName() ? sb_value.GetTypeName() : "<error_type>",
                    std::nullopt,
                    sb_value.GetDisplayTypeName() ? sb_value.GetDisplayTypeName() : "");
            }

            // 使用独立的转换方法设置变量值类型
            lldb::ValueType value_type = lldb::eValueTypeInvalid;
            try {
                value_type = sb_value.GetValueType();
            } catch (...) {
                // 如果获取值类型失败，使用默认值
                value_type = lldb::eValueTypeInvalid;
            }
            variable.set_value_kind(ConvertValueKind(value_type));

            // 设置是否有子变量（对应 SBValue::MightHaveChildren()）
            try {
                bool has_children = sb_value.MightHaveChildren();
                variable.set_has_children(has_children);
            } catch (...) {
                // 如果获取子变量信息失败，默认为 false
                variable.set_has_children(false);
            }

            try {
                uint64_t addr = sb_value.GetLoadAddress();
                variable.set_address(addr);
            } catch (...) {
                variable.set_address(0);
            }

            return variable;
        }

        lldbprotobuf::Value ProtoConverter::CreateValue(lldb::SBValue &sb_value, uint64_t variable_id) {
            return CreateValue(sb_value, variable_id, 1000);
        }

        lldbprotobuf::Value ProtoConverter::CreateValue(lldb::SBValue &sb_value,
                                                        uint64_t variable_id,
                                                        uint32_t max_string_length) {
            lldbprotobuf::Value value;

            value.mutable_variable_id()->set_id(variable_id);

            // 设置值的字符串表示
            if (const char *val = sb_value.GetValue()) {
                std::string val_str = val;
                // 限制字符串长度
                if (val_str.length() > max_string_length) {
                    val_str = val_str.substr(0, max_string_length) + "...";
                }
                value.set_value(val_str);
            } else {
                value.set_value("");
            }

            // 设置值的摘要（适用于复杂对象）
            if (const char *summary = sb_value.GetSummary()) {
                std::string summary_str = summary;
                // 限制摘要长度
                if (summary_str.length() > max_string_length) {
                    summary_str = summary_str.substr(0, max_string_length) + "...";
                }
                value.set_summary(summary_str);
            } else {
                value.set_summary("");
            }

            // 设置值是否发生了变化
            value.set_value_did_change(sb_value.GetValueDidChange());

            // 设置错误信息
            lldb::SBError error = sb_value.GetError();
            if (error.Fail()) {
                value.set_error(error.GetCString() ? error.GetCString() : "Unknown error");
            } else {
                value.set_error("");
            }

            // 注意：子变量信息通过 VariablesChildrenRequest 单独获取，以支持分页


            return value;
        }

        lldbprotobuf::TypeKind ProtoConverter::ConvertTypeKind(lldb::TypeClass type_class) {
            // 检查类型类别（注意：某些类型可能是多个类别的组合）
            if (type_class & lldb::eTypeClassArray) {
                return lldbprotobuf::TYPE_ARRAY;
            }
            if (type_class & lldb::eTypeClassBuiltin) {
                return lldbprotobuf::TYPE_BUILTIN;
            }
            if (type_class & lldb::eTypeClassClass) {
                return lldbprotobuf::TYPE_CLASS;
            }
            if (type_class & lldb::eTypeClassEnumeration) {
                return lldbprotobuf::TYPE_ENUM;
            }
            if (type_class & lldb::eTypeClassFunction) {
                return lldbprotobuf::TYPE_FUNCTION;
            }
            if (type_class & lldb::eTypeClassPointer) {
                return lldbprotobuf::TYPE_POINTER;
            }
            if (type_class & lldb::eTypeClassReference) {
                return lldbprotobuf::TYPE_REFERENCE;
            }
            if (type_class & lldb::eTypeClassStruct) {
                return lldbprotobuf::TYPE_STRUCT;
            }
            if (type_class & lldb::eTypeClassUnion) {
                return lldbprotobuf::TYPE_UNION;
            }
            if (type_class & lldb::eTypeClassTypedef) {
                return lldbprotobuf::TYPE_TYPEDEF;
            }
            if (type_class & lldb::eTypeClassVector) {
                return lldbprotobuf::TYPE_VECTOR;
            }
            if (type_class & lldb::eTypeClassBlockPointer) {
                return lldbprotobuf::TYPE_BLOCK_POINTER;
            }
            if (type_class & lldb::eTypeClassComplexFloat) {
                return lldbprotobuf::TYPE_COMPLEX_FLOAT;
            }
            if (type_class & lldb::eTypeClassComplexInteger) {
                return lldbprotobuf::TYPE_COMPLEX_INT;
            }
            if (type_class & lldb::eTypeClassMemberPointer) {
                return lldbprotobuf::TYPE_MEMBER_POINTER;
            }
            // 如果是多个类别的组合，优先级最高的在前面
            if (type_class & lldb::eTypeClassPointer) {
                return lldbprotobuf::TYPE_POINTER;
            }
            if (type_class & lldb::eTypeClassReference) {
                return lldbprotobuf::TYPE_REFERENCE;
            }
            if (type_class & lldb::eTypeClassArray) {
                return lldbprotobuf::TYPE_ARRAY;
            }
            if (type_class & (lldb::eTypeClassStruct | lldb::eTypeClassClass)) {
                return lldbprotobuf::TYPE_CLASS;
            }
            if (type_class & lldb::eTypeClassUnion) {
                return lldbprotobuf::TYPE_UNION;
            }
            if (type_class & lldb::eTypeClassEnumeration) {
                return lldbprotobuf::TYPE_ENUM;
            }
            if (type_class & lldb::eTypeClassBuiltin) {
                return lldbprotobuf::TYPE_BUILTIN;
            }
            if (type_class & lldb::eTypeClassFunction) {
                return lldbprotobuf::TYPE_FUNCTION;
            }
            if (type_class & lldb::eTypeClassTypedef) {
                return lldbprotobuf::TYPE_TYPEDEF;
            }
            if (type_class & lldb::eTypeClassVector) {
                return lldbprotobuf::TYPE_VECTOR;
            }
            {
                return lldbprotobuf::TYPE_OTHER;
            }
        }

        lldbprotobuf::ValueKind ProtoConverter::ConvertValueKind(lldb::ValueType value_type) {
            switch (value_type) {
                case lldb::eValueTypeVariableGlobal:
                    return lldbprotobuf::VALUE_GLOBAL;
                case lldb::eValueTypeVariableStatic:
                    return lldbprotobuf::VALUE_STATIC;
                case lldb::eValueTypeVariableArgument:
                    return lldbprotobuf::VALUE_ARGUMENT;
                case lldb::eValueTypeVariableLocal:
                    return lldbprotobuf::VALUE_LOCAL;
                case lldb::eValueTypeRegister:
                    return lldbprotobuf::VALUE_REGISTER;
                case lldb::eValueTypeRegisterSet:
                    return lldbprotobuf::VALUE_REGISTER_SET;
                case lldb::eValueTypeConstResult:
                    return lldbprotobuf::VALUE_CONST_RESULT;
                case lldb::eValueTypeVariableThreadLocal:
                    return lldbprotobuf::VALUE_THREAD_LOCAL;
                default:
                    return lldbprotobuf::VALUE_INVALID;
            }
        }

        // ============================================================================
        // 断点转换
        // ============================================================================

        lldbprotobuf::Breakpoint ProtoConverter::CreateBreakpoint(
            int64_t id,
            const lldbprotobuf::SourceLocation &original_location,
            const std::string &condition) {
            lldbprotobuf::Breakpoint breakpoint;
            *breakpoint.mutable_id() = CreateId(id);
            *breakpoint.mutable_original_location() = original_location;
            if (!condition.empty()) {
                breakpoint.set_condition(condition);
            }
            return breakpoint;
        }

        lldbprotobuf::BreakpointLocation ProtoConverter::CreateBreakpointLocation(
            int64_t id,
            uint64_t address,
            bool is_resolved,
            const lldbprotobuf::SourceLocation &location) {
            lldbprotobuf::BreakpointLocation bp_location;
            *bp_location.mutable_id() = CreateId(id);

            bp_location.set_address(address);
            bp_location.set_is_resolved(is_resolved);
            *bp_location.mutable_location() = location;
            return bp_location;
        }

        // ============================================================================
        // 响应消息创建
        // ============================================================================

        lldbprotobuf::Status ProtoConverter::CreateResponseStatus(
            bool success,
            const std::string &error_message) {
            lldbprotobuf::Status status;
            status.set_success(success);
            if (!error_message.empty()) {
                status.set_message(error_message);
            }
            return status;
        }

        lldbprotobuf::Id ProtoConverter::CreateId(const int64_t id) {
            lldbprotobuf::Id id1;
            id1.set_id(id);
            return id1;
        }


        lldbprotobuf::LaunchResponse ProtoConverter::CreateLaunchResponse(
            bool success,
            int64_t process_id,
            const std::string &error_message) {
            lldbprotobuf::LaunchResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            if (success) {
                *response.mutable_process() = CreateId(process_id);
            }
            return response;
        }

        lldbprotobuf::AttachResponse ProtoConverter::CreateAttachResponse(
            bool success,
            int64_t process_id,
            const std::string &error_message) {
            lldbprotobuf::AttachResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            if (success) {
                *response.mutable_process() = CreateId(process_id);
            }
            return response;
        }

        lldbprotobuf::DetachResponse ProtoConverter::CreateDetachResponse(bool success,
                                                                          const std::string &error_message) {
            lldbprotobuf::DetachResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::TerminateResponse ProtoConverter::CreateTerminateResponse(bool success,
            const std::string &error_message) {
            lldbprotobuf::TerminateResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::StepIntoResponse ProtoConverter::CreateStepIntoResponse(bool success,
                                                                              const std::string &error_message) {
            lldbprotobuf::StepIntoResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::StepOverResponse ProtoConverter::CreateStepOverResponse(bool success,
                                                                              const std::string &error_message) {
            lldbprotobuf::StepOverResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::StepOutResponse ProtoConverter::CreateStepOutResponse(bool success,
                                                                            const std::string &error_message) {
            lldbprotobuf::StepOutResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::RunToCursorResponse ProtoConverter::CreateRunToCursorResponse(
            bool success,
            uint64_t temp_breakpoint_id,
            const std::string &method_used,
            const std::string &error_message) {

            lldbprotobuf::RunToCursorResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (temp_breakpoint_id > 0) {
                *response.mutable_temp_breakpoint_id() = CreateId(temp_breakpoint_id);
            }

            response.set_method_used(method_used);
            return response;
        }

        lldbprotobuf::RemoveBreakpointResponse ProtoConverter::CreateRemoveBreakpointResponse(bool success,
            const std::string &error_message) {
            lldbprotobuf::RemoveBreakpointResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        // ========================================================================
        // 进程状态变更事件创建
        // ========================================================================

        lldbprotobuf::ProcessState ProtoConverter::ConvertProcessState(lldb::StateType state) {
            switch (state) {
                case lldb::eStateInvalid:
                    return lldbprotobuf::PROCESS_STATE_INVALID;
                case lldb::eStateUnloaded:
                    return lldbprotobuf::PROCESS_STATE_UNLOADED;
                case lldb::eStateConnected:
                    return lldbprotobuf::PROCESS_STATE_CONNECTED;
                case lldb::eStateAttaching:
                    return lldbprotobuf::PROCESS_STATE_ATTACHING;
                case lldb::eStateLaunching:
                    return lldbprotobuf::PROCESS_STATE_LAUNCHING;
                case lldb::eStateStopped:
                    return lldbprotobuf::PROCESS_STATE_STOPPED;
                case lldb::eStateRunning:
                    return lldbprotobuf::PROCESS_STATE_RUNNING;
                case lldb::eStateStepping:
                    return lldbprotobuf::PROCESS_STATE_STEPPING;
                case lldb::eStateCrashed:
                    return lldbprotobuf::PROCESS_STATE_CRASHED;
                case lldb::eStateDetached:
                    return lldbprotobuf::PROCESS_STATE_DETACHED;
                case lldb::eStateExited:
                    return lldbprotobuf::PROCESS_STATE_EXITED;
                case lldb::eStateSuspended:
                    return lldbprotobuf::PROCESS_STATE_SUSPENDED;
                default:
                    return lldbprotobuf::PROCESS_STATE_INVALID;
            }
        }

        lldbprotobuf::ProcessStateChanged ProtoConverter::CreateProcessStateChangedStopped(
            lldb::StateType state,
            const std::string &description,
            lldb::SBThread &stopped_thread,
            lldb::SBFrame &current_frame) {

            lldbprotobuf::ProcessStateChanged event;
            event.set_state(ConvertProcessState(state));
            event.set_description(description);

            // 填充停止详情
            auto* stopped_details = event.mutable_stopped_details();
            *stopped_details->mutable_stopped_thread() = CreateThread(stopped_thread);
            *stopped_details->mutable_current_frame() = CreateFrame(current_frame);

            return event;
        }

        lldbprotobuf::ProcessStateChanged ProtoConverter::CreateProcessStateChangedRunning(
            lldb::StateType state,
            const std::string &description,
            int64_t thread_id) {

            lldbprotobuf::ProcessStateChanged event;
            event.set_state(ConvertProcessState(state));
            event.set_description(description);

            // 填充运行详情
            auto* running_details = event.mutable_running_details();
            running_details->set_thread_id(thread_id);

            return event;
        }

        lldbprotobuf::ProcessStateChanged ProtoConverter::CreateProcessStateChangedExited(
            lldb::StateType state,
            const std::string &description,
            int32_t exit_code,
            const std::string &exit_description) {

            lldbprotobuf::ProcessStateChanged event;
            event.set_state(ConvertProcessState(state));
            event.set_description(description);

            // 填充退出详情
            auto* exited_details = event.mutable_exited_details();
            exited_details->set_exit_code(exit_code);
            if (!exit_description.empty()) {
                exited_details->set_description(exit_description);
            }

            return event;
        }

        lldbprotobuf::ProcessStateChanged ProtoConverter::CreateProcessStateChangedSimple(
            lldb::StateType state,
            const std::string &description) {

            lldbprotobuf::ProcessStateChanged event;
            event.set_state(ConvertProcessState(state));
            event.set_description(description);
            // 不填充任何详情，oneof 为空

            return event;
        }

        lldbprotobuf::Initialized ProtoConverter::CreateInitializedEvent(uint64_t uint64) {
            lldbprotobuf::Initialized initialized;
            initialized.set_capabilities(uint64);

            return initialized;
        }

        lldbprotobuf::ProcessOutput ProtoConverter::CreateProcessOutputEvent(
            const std::string &text,
            lldbprotobuf::OutputType output_type) {
            lldbprotobuf::ProcessOutput event;
            event.set_text(text);
            event.set_output_type(output_type);
            return event;
        }

        // 新增事件创建函数实现
        lldbprotobuf::ModuleEvent ProtoConverter::CreateModuleLoadedEvent(const std::vector<lldbprotobuf::Module> &modules) {
            lldbprotobuf::ModuleEvent event;
            event.set_event_type(lldbprotobuf::MODULE_LOADED);
            for (const auto &module : modules) {
                *event.add_modules() = module;
            }
            return event;
        }

        lldbprotobuf::ModuleEvent ProtoConverter::CreateModuleUnloadedEvent(const std::vector<lldbprotobuf::Module> &modules) {
            lldbprotobuf::ModuleEvent event;
            event.set_event_type(lldbprotobuf::MODULE_UNLOADED);
            for (const auto &module : modules) {
                *event.add_modules() = module;
            }
            return event;
        }

        lldbprotobuf::BreakpointChangedEvent ProtoConverter::CreateBreakpointChangedEvent(
            const lldbprotobuf::Breakpoint &breakpoint,
            lldbprotobuf::BreakpointEventType change_type,
            const std::string &description) {
            lldbprotobuf::BreakpointChangedEvent event;
            *event.mutable_breakpoint() = breakpoint;
            event.set_change_type(change_type);
            if (!description.empty()) {
                event.set_description(description);
            }
            return event;
        }

        lldbprotobuf::ThreadStateChangedEvent ProtoConverter::CreateThreadStateChangedEvent(
            const lldbprotobuf::Thread &thread,
            lldbprotobuf::ThreadStateChangeType change_type,
            const std::string &description) {
            lldbprotobuf::ThreadStateChangedEvent event;
            *event.mutable_thread() = thread;
            event.set_change_type(change_type);
            if (!description.empty()) {
                event.set_description(description);
            }
            return event;
        }

        lldbprotobuf::SymbolsLoadedEvent ProtoConverter::CreateSymbolsLoadedEvent(
            const lldbprotobuf::Module &module,
            uint32_t symbol_count,
            const std::string &symbol_file_path) {
            lldbprotobuf::SymbolsLoadedEvent event;
            *event.mutable_module() = module;
            event.set_symbol_count(symbol_count);
            if (!symbol_file_path.empty()) {
                event.set_symbol_file_path(symbol_file_path);
            }
            return event;
        }

        lldbprotobuf::ContinueResponse ProtoConverter::CreateContinueResponse(bool cond, const char *str) {
            lldbprotobuf::ContinueResponse continue_response;
            *continue_response.mutable_status() = CreateResponseStatus(cond, str);

            return continue_response;
        }

        lldbprotobuf::SuspendResponse ProtoConverter::CreateSuspendResponse(bool cond, const char *str) {
            lldbprotobuf::SuspendResponse continue_response;
            *continue_response.mutable_status() = CreateResponseStatus(cond, str);

            return continue_response;
        }

        lldbprotobuf::ExitResponse ProtoConverter::CreateExitResponse(bool success, const char *str) {
            lldbprotobuf::ExitResponse continue_response;
            *continue_response.mutable_status() = CreateResponseStatus(success, str);

            return continue_response;
        }

        // ============================================================================
        // 断点响应创建方法
        // ============================================================================

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateAddBreakpointResponse(
            bool success,
            BreakpointType breakpoint_type,
            const lldbprotobuf::Breakpoint &breakpoint,
            const std::vector<lldbprotobuf::BreakpointLocation> &locations,
            const std::string &error_message) {
            lldbprotobuf::AddBreakpointResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (!success) {
                return response;
            }

            // 根据断点类型设置对应的oneof字段
            switch (breakpoint_type) {
                case BreakpointType::LINE_BREAKPOINT: {
                    auto *line_result = response.mutable_line_breakpoint();
                    *line_result->mutable_breakpoint() = breakpoint;
                    for (const auto &location: locations) {
                        *line_result->add_locations() = location;
                    }
                    break;
                }
                case BreakpointType::ADDRESS_BREAKPOINT: {
                    auto *address_result = response.mutable_address_breakpoint();
                    *address_result->mutable_breakpoint() = breakpoint;
                    for (const auto &location: locations) {
                        *address_result->add_locations() = location;
                    }
                    break;
                }
                case BreakpointType::FUNCTION_BREAKPOINT: {
                    auto *function_result = response.mutable_function_breakpoint();
                    *function_result->mutable_breakpoint() = breakpoint;
                    for (const auto &location: locations) {
                        *function_result->add_locations() = location;
                    }
                    break;
                }
                case BreakpointType::WATCH_BREAKPOINT: {
                    auto *watch_result = response.mutable_watchpoint();
                    *watch_result->mutable_break_point_id() = CreateId(breakpoint.id().id());
                    break;
                }
                case BreakpointType::SYMBOL_BREAKPOINT: {
                    auto *symbol_result = response.mutable_symbol_breakpoint();
                    *symbol_result->mutable_breakpoint() = breakpoint;
                    for (const auto &location: locations) {
                        *symbol_result->add_locations() = location;
                    }
                    break;
                }
                default:
                    break;
            }

            return response;
        }

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateLineBreakpointResponse(
            bool success,
            const lldbprotobuf::Breakpoint &breakpoint,
            const std::vector<lldbprotobuf::BreakpointLocation> &locations,
            const std::string &error_message) {
            return CreateAddBreakpointResponse(
                success,
                BreakpointType::LINE_BREAKPOINT,
                breakpoint,
                locations,
                error_message);
        }

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateAddressBreakpointResponse(
            bool success,
            const lldbprotobuf::Breakpoint &breakpoint,
            const std::vector<lldbprotobuf::BreakpointLocation> &locations,
            const std::string &error_message) {
            return CreateAddBreakpointResponse(
                success,
                BreakpointType::ADDRESS_BREAKPOINT,
                breakpoint,
                locations,
                error_message);
        }

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateFunctionBreakpointResponse(
            bool success,
            const lldbprotobuf::Breakpoint &breakpoint,
            const std::vector<lldbprotobuf::BreakpointLocation> &locations,
            const std::string &error_message) {
            return CreateAddBreakpointResponse(
                success,
                BreakpointType::FUNCTION_BREAKPOINT,
                breakpoint,
                locations,
                error_message);
        }

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateWatchpointResponse(
            bool success,
            int64_t watchpoint_id,
            const std::string &error_message) {
            // 为观察点创建一个虚拟的breakpoint对象
            lldbprotobuf::Breakpoint virtual_breakpoint;
            *virtual_breakpoint.mutable_id() = CreateId(watchpoint_id);

            return CreateAddBreakpointResponse(
                success,
                BreakpointType::WATCH_BREAKPOINT,
                virtual_breakpoint,
                {},
                error_message);
        }

        lldbprotobuf::AddBreakpointResponse ProtoConverter::CreateSymbolBreakpointResponse(
            bool success,
            const lldbprotobuf::Breakpoint &breakpoint,
            const std::vector<lldbprotobuf::BreakpointLocation> &locations,
            const std::string &error_message) {
            return CreateAddBreakpointResponse(
                success,
                BreakpointType::SYMBOL_BREAKPOINT,
                breakpoint,
                locations,
                error_message);
        }


        // ============================================================================
        // 事件消息创建
        // ============================================================================


        // ============================================================================
        // 基础响应创建
        // ============================================================================

        lldbprotobuf::CreateTargetResponse ProtoConverter::CreateCreateTargetResponse(
            bool success,
            const std::string &error_message) {
            lldbprotobuf::CreateTargetResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::ThreadsResponse ProtoConverter::CreateThreadsResponse(
            bool success,
            const std::vector<lldbprotobuf::Thread> &threads,
            const std::string &error_message) {
            lldbprotobuf::ThreadsResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            // 添加所有线程
            for (const auto &thread: threads) {
                *response.add_threads() = thread;
            }

            return response;
        }

        lldbprotobuf::FramesResponse ProtoConverter::CreateFramesResponse(
            bool success,
            const std::vector<lldbprotobuf::Frame> &frames,
            uint32_t total_frames,
            const std::string &error_message) {
            lldbprotobuf::FramesResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            // 设置总帧数
            response.set_total_frames(total_frames);

            // 添加所有帧
            for (const auto &frame: frames) {
                *response.add_frames() = frame;
            }

            return response;
        }

        lldbprotobuf::VariablesResponse ProtoConverter::CreateVariablesResponse(
            bool success,
            const std::vector<lldbprotobuf::Variable> &variables,
            const std::string &error_message) {
            lldbprotobuf::VariablesResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            // 添加所有变量
            for (const auto &variable: variables) {
                *response.add_variables() = variable;
            }

            return response;
        }

        lldbprotobuf::GetValueResponse ProtoConverter::CreateGetValueResponse(
            bool success,
            const lldbprotobuf::Value &value,
            const lldbprotobuf::Variable &variable,
            const std::string &error_message) {
            lldbprotobuf::GetValueResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                *response.mutable_value() = value;
                *response.mutable_variable() = variable;
            }

            return response;
        }

        lldbprotobuf::SetVariableValueResponse ProtoConverter::CreateSetVariableValueResponse(
            bool success,
            const lldbprotobuf::Value &value,
            const lldbprotobuf::Variable &variable,
            const std::string &error_message) {
            lldbprotobuf::SetVariableValueResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                *response.mutable_value() = value;
                *response.mutable_variable() = variable;
            }

            return response;
        }

        lldbprotobuf::VariablesChildrenResponse ProtoConverter::CreateVariablesChildrenResponse(
            bool success,
            const std::vector<lldbprotobuf::Variable> &children,
            uint32_t total_children,
            uint32_t offset,
            bool has_more,
            const std::string &error_message) {
            lldbprotobuf::VariablesChildrenResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                // 添加所有子变量
                for (const auto &child: children) {
                    *response.add_children() = child;
                }

                response.set_total_children(total_children);
                response.set_offset(offset);
                response.set_has_more(has_more);
            }

            return response;
        }

        lldbprotobuf::EvaluateResponse ProtoConverter::CreateEvaluateResponse(
            bool success,
            const lldbprotobuf::Variable &variable,
            const std::string &error_message) {
            lldbprotobuf::EvaluateResponse response;


            if (success) {
                *response.mutable_result() = variable;
            }

            *response.mutable_status() = CreateResponseStatus(success, error_message);
            return response;
        }

        lldbprotobuf::ReadMemoryResponse ProtoConverter::CreateReadMemoryResponse(
            bool success,

            const std::string &data,
            const std::string &error_message) {
            lldbprotobuf::ReadMemoryResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                response.set_data(data);
                // Note: ReadMemoryResponse doesn't have address field according to protobuf definition
                // Address information should be included in the calling context if needed
            }

            return response;
        }

        lldbprotobuf::WriteMemoryResponse ProtoConverter::CreateWriteMemoryResponse(
            bool success,
            uint32_t bytes_written,
            const std::string &error_message) {
            lldbprotobuf::WriteMemoryResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                response.set_bytes_written(bytes_written);
            }

            return response;
        }

        lldbprotobuf::DisassembleResponse ProtoConverter::CreateDisassembleResponse(
            bool success,
            const std::vector<lldbprotobuf::DisassembleInstruction>& instructions,
            uint32_t bytes_disassembled,
            bool alignment_verified,
            uint64_t actual_end_address,
            const std::string &error_message) {
            lldbprotobuf::DisassembleResponse response;
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                // 添加反汇编指令列表
                for (const auto& instruction : instructions) {
                    auto* new_instruction = response.add_instructions();
                    *new_instruction = instruction;
                }
                response.set_bytes_disassembled(bytes_disassembled);
                response.set_alignment_verified(alignment_verified);
                response.set_actual_end_address(actual_end_address);
            }

            return response;
        }

        // ============================================================================
        // 寄存器转换
        // ============================================================================

        lldbprotobuf::Register ProtoConverter::CreateRegister(lldb::SBValue &sb_value) {
            lldbprotobuf::Register reg;

            // 设置寄存器名称
            if (const char *name = sb_value.GetName()) {
                reg.set_name(name);
            } else {
                reg.set_name("<unnamed_register>");
            }

            // 设置寄存器值（十六进制字符串）
            if (const char *value = sb_value.GetValue()) {
                reg.set_value(value);

                // 设置无符号整数值（如果可以解析的话）
                try {
                    std::string value_str = value;
                    if (value_str.find("0x") == 0 || value_str.find("0X") == 0) {
                        // 尝试解析十六进制值
                        uint64_t unsigned_val = std::stoull(value_str, nullptr, 16);
                        reg.set_value_unsigned(unsigned_val);
                    }
                } catch (...) {
                    // 如果解析失败，设置默认值0
                    reg.set_value_unsigned(0);
                }
            } else {
                reg.set_value("0x0");
                reg.set_value_unsigned(0);
            }

            // 设置寄存器大小（字节）
            try {
                reg.set_size(sb_value.GetByteSize());
            } catch (...) {
                // 如果获取大小失败，根据寄存器名称推断
                std::string reg_name = reg.name();
                if (reg_name.find("r") == 0 && reg_name.length() >= 2 && reg_name.length() <= 3) {
                    // 64位通用寄存器 (rax, rbx, etc.)
                    reg.set_size(8);
                } else if (reg_name.find("e") == 0 && reg_name.length() >= 2 && reg_name.length() <= 3) {
                    // 32位寄存器 (eax, ebx, etc.)
                    reg.set_size(4);
                } else if (reg_name.find("xmm") == 0) {
                    // XMM寄存器 (128位)
                    reg.set_size(16);
                } else if (reg_name.find("ymm") == 0) {
                    // YMM寄存器 (256位)
                    reg.set_size(32);
                } else if (reg_name.find("zmm") == 0) {
                    // ZMM寄存器 (512位)
                    reg.set_size(64);
                } else {
                    reg.set_size(8); // 默认8字节
                }
            }

            // 设置类型名称
            if (const char *type_name = sb_value.GetTypeName()) {
                reg.set_type_name(type_name);
            } else {
                reg.set_type_name("register");
            }

            // 设置寄存器摘要（对于大寄存器提供摘要）
            if (const char *summary = sb_value.GetSummary()) {
                reg.set_summary(summary);
            } else {
                // 如果没有摘要，尝试使用值作为摘要
                if (const char *value = sb_value.GetValue()) {
                    reg.set_summary(value);
                }
            }

            // 获取寄存器集合名称
            // 寄存器集合通常通过寄存器名称的前缀来判断
            std::string reg_name = reg.name();
            if (reg_name.find("rax") != std::string::npos ||
                reg_name.find("rbx") != std::string::npos ||
                reg_name.find("rcx") != std::string::npos ||
                reg_name.find("rdx") != std::string::npos ||
                reg_name.find("rsi") != std::string::npos ||
                reg_name.find("rdi") != std::string::npos ||
                reg_name.find("rbp") != std::string::npos ||
                reg_name.find("rsp") != std::string::npos ||
                reg_name.find("r8") != std::string::npos ||
                reg_name.find("r9") != std::string::npos ||
                reg_name.find("r10") != std::string::npos ||
                reg_name.find("r11") != std::string::npos ||
                reg_name.find("r12") != std::string::npos ||
                reg_name.find("r13") != std::string::npos ||
                reg_name.find("r14") != std::string::npos ||
                reg_name.find("r15") != std::string::npos) {
                reg.set_group_name("general");
            } else if (reg_name.find("xmm") != std::string::npos ||
                      reg_name.find("ymm") != std::string::npos ||
                      reg_name.find("zmm") != std::string::npos) {
                reg.set_group_name("floating_point");
            } else if (reg_name.find("cs") != std::string::npos ||
                      reg_name.find("ds") != std::string::npos ||
                      reg_name.find("es") != std::string::npos ||
                      reg_name.find("fs") != std::string::npos ||
                      reg_name.find("gs") != std::string::npos ||
                      reg_name.find("ss") != std::string::npos) {
                reg.set_group_name("special");
            } else if (reg_name.find("st") != std::string::npos) {
                reg.set_group_name("floating_point");
            } else if (reg_name.find("mm") != std::string::npos) {
                reg.set_group_name("vector");
            } else {
                reg.set_group_name("general");
            }

            // 设置是否有子元素（对于向量寄存器等）
            try {
                reg.set_has_children(sb_value.MightHaveChildren());
            } catch (...) {
                // 向量寄存器（如XMM, YMM, ZMM）通常有子元素
                if (reg_name.find("xmm") == 0 || reg_name.find("ymm") == 0 || reg_name.find("zmm") == 0) {
                    reg.set_has_children(true);
                } else {
                    reg.set_has_children(false);
                }
            }

            // 设置值是否发生变化
            try {
                reg.set_changed(sb_value.GetValueDidChange());
            } catch (...) {
                reg.set_changed(false);
            }

            return reg;
        }

        lldbprotobuf::RegisterGroup ProtoConverter::CreateRegisterGroup(
            const std::string &name,
            uint32_t register_count) {
            lldbprotobuf::RegisterGroup group;

            // RegisterGroup protobuf 消息只有 name 和 register_count 字段
            group.set_name(name);
            group.set_register_count(register_count);

            return group;
        }

        lldbprotobuf::RegistersResponse ProtoConverter::CreateRegistersResponse(
            bool success,
            const std::vector<lldbprotobuf::Register> &registers,
            const std::string &error_message) {
            lldbprotobuf::RegistersResponse response;

            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                // 添加所有寄存器
                for (const auto &reg : registers) {
                    *response.add_registers() = reg;
                }
            }

            return response;
        }

        lldbprotobuf::RegisterGroupsResponse ProtoConverter::CreateRegisterGroupsResponse(
            bool success,
            const std::vector<lldbprotobuf::RegisterGroup> &register_groups,
            const std::string &error_message) {
            lldbprotobuf::RegisterGroupsResponse response;

            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success) {
                // 添加所有寄存器组
                for (const auto &group : register_groups) {
                    *response.add_groups() = group;
                }
            }

            return response;
        }

        // ============================================================================
        // 函数信息转换
        // ============================================================================

        lldbprotobuf::FunctionInfo ProtoConverter::CreateFunctionInfo(
              lldb::SBFunction& function,
            const lldb::SBTarget& target) {
            lldbprotobuf::FunctionInfo info;

            // 设置函数名
            if (const char* name = function.GetName()) {
                info.set_name(name);
            } else {
                info.set_name("<unknown>");
            }

            // 设置修饰名
            if (const char* mangled = function.GetMangledName()) {
                info.set_mangled_name(mangled);
            } else {
                info.set_mangled_name("");
            }

            // 获取地址信息
            lldb::SBAddress start_addr = function.GetStartAddress();
            lldb::SBAddress end_addr = function.GetEndAddress();

            if (start_addr.IsValid()) {
                uint64_t start = start_addr.GetLoadAddress(target);
                info.set_start_address(start);

                if (end_addr.IsValid()) {
                    uint64_t end = end_addr.GetLoadAddress(target);
                    info.set_end_address(end);
                    info.set_size(end - start);
                } else {
                    info.set_end_address(start);
                    info.set_size(0);
                }
            }

            // 获取源码位置
            lldb::SBLineEntry line_entry = start_addr.GetLineEntry();
            if (line_entry.IsValid()) {
                lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
                if (file_spec.IsValid()) {
                    char file_path[1024];
                    file_spec.GetPath(file_path, sizeof(file_path));
                    *info.mutable_location() = CreateSourceLocation(file_path, line_entry.GetLine());
                }
            }

            // 获取模块信息
            lldb::SBModule module = start_addr.GetModule();
            if (module.IsValid()) {
                if (const char* module_name = module.GetFileSpec().GetFilename()) {
                    info.set_module_name(module_name);
                }
            }

            // 获取语言类型
            lldb::LanguageType lang_type = function.GetLanguage();
            switch (lang_type) {
                case lldb::eLanguageTypeC:
                    info.set_language("c");
                    break;
                case lldb::eLanguageTypeC_plus_plus:
                    info.set_language("c++");
                    break;
                case lldb::eLanguageTypeC99:
                    info.set_language("c99");
                    break;
                case lldb::eLanguageTypeC11:
                    info.set_language("c11");
                    break;
                case lldb::eLanguageTypeC_plus_plus_03:
                    info.set_language("c++03");
                    break;
                case lldb::eLanguageTypeC_plus_plus_11:
                    info.set_language("c++11");
                    break;
                case lldb::eLanguageTypeC_plus_plus_14:
                    info.set_language("c++14");
                    break;
                case lldb::eLanguageTypeRust:
                    info.set_language("rust");
                    break;
                case lldb::eLanguageTypeSwift:
                    info.set_language("swift");
                    break;
                default:
                    info.set_language("unknown");
                    break;
            }

            // 设置 is_external: 检查是否有调试信息
            info.set_is_external(!line_entry.IsValid());

            // 设置 is_hole: 暂时设为 false
            info.set_is_hole(false);

            return info;
        }

        lldbprotobuf::FunctionInfo ProtoConverter::CreateFunctionInfoFromSymbol(
              lldb::SBSymbol& symbol,
            const lldb::SBTarget& target) {
            lldbprotobuf::FunctionInfo info;

            // 设置函数名
            if (const char* name = symbol.GetName()) {
                info.set_name(name);
            } else {
                info.set_name("<unknown>");
            }

            // 设置修饰名
            if (const char* mangled = symbol.GetMangledName()) {
                info.set_mangled_name(mangled);
            } else {
                info.set_mangled_name("");
            }

            // 获取地址信息
            lldb::SBAddress start_addr = symbol.GetStartAddress();
            lldb::SBAddress end_addr = symbol.GetEndAddress();

            if (start_addr.IsValid()) {
                uint64_t start = start_addr.GetLoadAddress(target);
                info.set_start_address(start);

                if (end_addr.IsValid()) {
                    uint64_t end = end_addr.GetLoadAddress(target);
                    info.set_end_address(end);
                    info.set_size(end - start);
                } else {
                    info.set_end_address(start);
                    info.set_size(0);
                }
            }

            // 符号通常没有源码位置信息
            *info.mutable_location() = CreateSourceLocation("", 0);

            // 获取模块信息
            lldb::SBModule module = start_addr.GetModule();
            if (module.IsValid()) {
                if (const char* module_name = module.GetFileSpec().GetFilename()) {
                    info.set_module_name(module_name);
                }
            }

            // 符号通常无语言类型信息
            info.set_language("unknown");

            // 符号是外部符号（无调试信息）
            info.set_is_external(true);

            // 设置 is_hole: 暂时设为 false
            info.set_is_hole(false);

            return info;
        }

        lldbprotobuf::GetFunctionInfoResponse ProtoConverter::CreateGetFunctionInfoResponse(
            bool success,
            const std::vector<lldbprotobuf::FunctionInfo>& functions,
            const std::string& error_message) {
            lldbprotobuf::GetFunctionInfoResponse response;

            *response.mutable_status() = CreateResponseStatus(success, error_message);

            if (success && !functions.empty()) {
                // 如果只有一个结果，设置到 function 字段
                if (functions.size() == 1) {
                    *response.mutable_function() = functions[0];
                }

                // 所有结果都添加到 functions 列表
                for (const auto& func : functions) {
                    *response.add_functions() = func;
                }
            }

            return response;
        }

        // ============================================================================
        // 控制台命令响应创建
        // ============================================================================

        lldbprotobuf::ExecuteCommandResponse ProtoConverter::CreateExecuteCommandResponse(
            bool success,
            const std::string &output,
            const std::string &error_output,
            int32_t return_status,
            const std::string &error_message) {
            lldbprotobuf::ExecuteCommandResponse response;

            // 设置操作状态
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            // 设置命令输出
            response.set_output(output);

            // 设置错误输出
            response.set_error_output(error_output);

            // 设置 LLDB 返回状态
            response.set_return_status(return_status);

            return response;
        }

        lldbprotobuf::CommandCompletionResponse ProtoConverter::CreateCommandCompletionResponse(
            bool success,
            const std::vector<std::string> &completions,
            const std::string &common_prefix,
            uint32_t completion_start,
            bool has_more,
            const std::string &error_message) {
            lldbprotobuf::CommandCompletionResponse response;

            // 设置操作状态
            *response.mutable_status() = CreateResponseStatus(success, error_message);

            // 设置补全候选列表
            if (success) {
                for (const auto &completion : completions) {
                    response.add_completions(completion);
                }

                // 设置共同前缀
                response.set_common_prefix(common_prefix);

                // 设置补全起始位置
                response.set_completion_start(completion_start);

                // 设置是否还有更多结果
                response.set_has_more(has_more);
            }

            return response;
        }
    } // namespace Debugger
} // namespace Cangjie
