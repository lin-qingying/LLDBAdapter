// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_DEBUGGER_PROTO_CONVERTER_H
#define CANGJIE_DEBUGGER_PROTO_CONVERTER_H

#include "model.pb.h"
#include "event.pb.h"

#include "response.pb.h"

#include <string>
#include <vector>

#include <optional>

#include "lldb/API/SBFrame.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBValue.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBWatchpoint.h"




namespace Cangjie {
    namespace Debugger {
        /**
         * @brief 断点类型枚举
         *
         * 定义不同类型的断点，用于创建断点响应时的类型区分
         */
        enum class BreakpointType {
            LINE_BREAKPOINT = 0, // 行断点
            ADDRESS_BREAKPOINT = 1, // 地址断点
            FUNCTION_BREAKPOINT = 2, // 函数断点
            WATCH_BREAKPOINT = 3, // 观察点（数据断点）
            SYMBOL_BREAKPOINT = 4 // 符号断点
        };

        /**
         * @brief Proto消息转换工具类
         *
         * 提供在优化后的proto消息类型和内部数据结构之间进行转换的功能
         */
        class ProtoConverter {
        public:
            // ========================================================================
            // 基础类型转换
            // ========================================================================

            /**
             * @brief 创建环境变量消息
             */
            static lldbprotobuf::EnvironmentVariable CreateEnvironmentVariable(
                const std::string &name,
                const std::string &value);


            static lldbprotobuf::Id CreateId(
                const int64_t id
            );

            static lldbprotobuf::Hash CreateHash(lldbprotobuf::HashAlgorithm hash_algorithm, const std::string &string);

            /**
             * @brief 创建源代码位置信息
             */
            static lldbprotobuf::SourceLocation CreateSourceLocation(
                const std::string &file_path,
                uint32_t line,
                lldbprotobuf::HashAlgorithm hash_algorithm = lldbprotobuf::HASH_ALGORITHM_NONE,
                const std::string &hash_value = "");

            // ========================================================================
            // 线程和执行状态转换
            // ========================================================================

            /**
             * @brief 从LLDB停止原因创建protobuf停止原因
             */
            static lldbprotobuf::StopReason CreateStopReason(lldb::StopReason lldb_stop_reason);

            /**
             * @brief 创建线程停止信息
             */
            static lldbprotobuf::ThreadStopInfo CreateThreadStopInfo(lldb::SBThread &sb_thread);

            /**
             * @brief 获取信号名称
             */
            static std::string GetSignalName(int32_t signal_num);

            /**
             * @brief 判断是否为致命信号
             */
            static bool IsFatalSignal(int32_t signal_num);


            /**
             * @brief 创建进程停止事件
             */
            static lldbprotobuf::ProcessStopped CreateProcessStopped(lldb::SBThread &stopped_thread,
                                                                     lldb::SBFrame &current_frame);

            /**
             * @brief 创建线程信息
             */
            static lldbprotobuf::Thread CreateThread(lldb::SBThread &sb_thread);

            /**
             * @brief 创建栈帧信息
             */
            static lldbprotobuf::Frame CreateFrame(lldb::SBFrame &sb_frame);

            static lldbprotobuf::Type CreateType(lldb::SBType &sb_type);

            static lldbprotobuf::Type CreateType(std::string type_name, std::optional<lldbprotobuf::TypeKind> type_kind,
                                                 std::string display_type);


            /**
             * @brief 将 LLDB TypeClass 转换为 protobuf TypeKind
             */
            static lldbprotobuf::TypeKind ConvertTypeKind(lldb::TypeClass type_class);

            /**
             * @brief 将 LLDB ValueType 转换为 protobuf ValueKind
             */
            static lldbprotobuf::ValueKind ConvertValueKind(lldb::ValueType value_type);

            /**
             * @brief 创建变量信息
             */
            static lldbprotobuf::Variable CreateVariable(lldb::SBValue &sb_value, uint64_t variable_id);

            /**
             * @brief 创建变量值信息
             */
            static lldbprotobuf::Value CreateValue(lldb::SBValue &sb_value, uint64_t variable_id);

            /**
             * @brief 创建变量值信息（带限制参数）
             */
            static lldbprotobuf::Value CreateValue(lldb::SBValue &sb_value,
                                                   uint64_t variable_id,
                                                   uint32_t max_string_length = 1000);


            // ========================================================================
            // 断点转换
            // ========================================================================

            /**
             * @brief 创建断点信息
             */
            static lldbprotobuf::Breakpoint CreateBreakpoint(
                int64_t id,
                const lldbprotobuf::SourceLocation &original_location,
                const std::string &condition = "");

            /**
             * @brief 创建断点位置信息
             */
            static lldbprotobuf::BreakpointLocation CreateBreakpointLocation(
                int64_t id,
                uint64_t address,
                bool is_resolved,
                const lldbprotobuf::SourceLocation &location);

            // ========================================================================
            // 响应消息创建
            // ========================================================================

            /**
             * @brief 创建响应状态
             */
            static lldbprotobuf::Status CreateResponseStatus(
                bool success,
                const std::string &error_message = "");

            static lldbprotobuf::CreateTargetResponse CreateCreateTargetResponse(
                bool success,
                const std::string &error_message);

            /**
             * @brief 创建启动响应
             */
            static lldbprotobuf::LaunchResponse CreateLaunchResponse(
                bool success,
                int64_t process_id,
                const std::string &error_message = "");

            /**
             * @brief 创建附加响应
             */
            static lldbprotobuf::AttachResponse CreateAttachResponse(
                bool success,
                int64_t process_id,
                const std::string &error_message = "");

            /**
             * @brief 创建分离响应
             */
            static lldbprotobuf::DetachResponse CreateDetachResponse(
                bool success,
                const std::string &error_message = "");

            /**
             * @brief 创建终止响应
             */
            static lldbprotobuf::TerminateResponse CreateTerminateResponse(
                bool success,
                const std::string &error_message = "");


            /**
             * @brief 创建单步进入响应
             */
            static lldbprotobuf::StepIntoResponse CreateStepIntoResponse(
                bool success,
                const std::string &error_message = "");

            /**
             * @brief 创建单步跳过响应
             */
            static lldbprotobuf::StepOverResponse CreateStepOverResponse(
                bool success,
                const std::string &error_message = "");

            /**
             * @brief 创建单步跳出响应
             */
            static lldbprotobuf::StepOutResponse CreateStepOutResponse(
                bool success,
                const std::string &error_message = "");

            /**
             * @brief 创建运行到光标处响应
             */
            static lldbprotobuf::RunToCursorResponse CreateRunToCursorResponse(
                bool success,
                uint64_t temp_breakpoint_id = 0,
                const std::string &method_used = "",
                const std::string &error_message = "");

            /**
             * @brief 创建移除断点响应
             */
            static lldbprotobuf::RemoveBreakpointResponse CreateRemoveBreakpointResponse(
                bool success,
                const std::string &error_message = "");


            // ========================================================================
            // 事件消息创建
            // ========================================================================


            /**
             * @brief 创建进程退出事件
             */
            static lldbprotobuf::ProcessExited CreateProcessExitedEvent(
                int32_t exit_code,
                const std::string &exit_description = "");

            static lldbprotobuf::Initialized CreateInitializedEvent(uint64_t uint64);

            /**
             * @brief 创建进程输出事件
             */
            static lldbprotobuf::ProcessOutput CreateProcessOutputEvent(
                const std::string &text,
                lldbprotobuf::OutputType output_type);

            // 新增事件创建函数
            static lldbprotobuf::ModuleEvent CreateModuleLoadedEvent(const std::vector<lldbprotobuf::Module> &modules);
            static lldbprotobuf::ModuleEvent CreateModuleUnloadedEvent(const std::vector<lldbprotobuf::Module> &modules);
            static lldbprotobuf::BreakpointChangedEvent CreateBreakpointChangedEvent(
                const lldbprotobuf::Breakpoint &breakpoint,
                lldbprotobuf::BreakpointEventType change_type,
                const std::string &description = "");
            static lldbprotobuf::ThreadStateChangedEvent CreateThreadStateChangedEvent(
                const lldbprotobuf::Thread &thread,
                lldbprotobuf::ThreadStateChangeType change_type,
                const std::string &description = "");
            static lldbprotobuf::SymbolsLoadedEvent CreateSymbolsLoadedEvent(
                const lldbprotobuf::Module &module,
                uint32_t symbol_count,
                const std::string &symbol_file_path = "");

            static lldbprotobuf::ContinueResponse CreateContinueResponse(bool cond, const char *str);

            static lldbprotobuf::SuspendResponse CreateSuspendResponse(bool cond, const char *str);

            static lldbprotobuf::ExitResponse CreateExitResponse(bool success, const char *str);

            static lldbprotobuf::ThreadsResponse CreateThreadsResponse(
                bool success,
                const std::vector<lldbprotobuf::Thread> &threads = {},
                const std::string &error_message = "");

            static lldbprotobuf::FramesResponse CreateFramesResponse(
                bool success,
                const std::vector<lldbprotobuf::Frame> &frames = {},
                uint32_t total_frames = 0,
                const std::string &error_message = "");

            static lldbprotobuf::VariablesResponse CreateVariablesResponse(
                bool success,
                const std::vector<lldbprotobuf::Variable> &variables = {},
                const std::string &error_message = "");

            static lldbprotobuf::GetValueResponse CreateGetValueResponse(
                bool success,
                const lldbprotobuf::Value &value,
                const lldbprotobuf::Variable &variable,
                const std::string &error_message = "");

            static lldbprotobuf::SetVariableValueResponse CreateSetVariableValueResponse(
                bool success,
                const lldbprotobuf::Value &value,
                const lldbprotobuf::Variable &variable,
                const std::string &error_message = "");

            static lldbprotobuf::VariablesChildrenResponse CreateVariablesChildrenResponse(
                bool success,
                const std::vector<lldbprotobuf::Variable> &children = {},
                uint32_t total_children = 0,
                uint32_t offset = 0,
                bool has_more = false,
                const std::string &error_message = "");

            static lldbprotobuf::EvaluateResponse CreateEvaluateResponse(
                bool success,
                const lldbprotobuf::Variable &variable,
                const std::string &error_message);

            static lldbprotobuf::ReadMemoryResponse CreateReadMemoryResponse(
                bool success,

                const std::string &data,
                const std::string &error_message = "");

            static lldbprotobuf::WriteMemoryResponse CreateWriteMemoryResponse(
                bool success,
                uint32_t bytes_written,
                const std::string &error_message = "");

            static lldbprotobuf::DisassembleResponse CreateDisassembleResponse(
                bool success,
                const std::vector<lldbprotobuf::DisassembleInstruction> &instructions = {},
                uint32_t bytes_disassembled = 0,
                bool alignment_verified = false,
                uint64_t actual_end_address = 0,
                const std::string &error_message = "");

            // ========================================================================
            // 函数信息转换
            // ========================================================================

            /**
             * @brief 从 LLDB 函数对象创建函数信息
             */
            static lldbprotobuf::FunctionInfo CreateFunctionInfo(
                  lldb::SBFunction &function,
                const lldb::SBTarget &target);

            /**
             * @brief 从 LLDB 符号对象创建函数信息
             */
            static lldbprotobuf::FunctionInfo CreateFunctionInfoFromSymbol(
                  lldb::SBSymbol &symbol,
                const lldb::SBTarget &target);

            /**
             * @brief 创建函数信息查询响应
             */
            static lldbprotobuf::GetFunctionInfoResponse CreateGetFunctionInfoResponse(
                bool success,
                const std::vector<lldbprotobuf::FunctionInfo> &functions = {},
                const std::string &error_message = "");

            // ========================================================================
            // 寄存器转换
            // ========================================================================

            /**
             * @brief 创建寄存器信息
             */
            static lldbprotobuf::Register CreateRegister(lldb::SBValue &sb_value);

            /**
             * @brief 创建寄存器组信息
             */
            static lldbprotobuf::RegisterGroup CreateRegisterGroup(
                const std::string &name,
                uint32_t register_count);

            /**
             * @brief 创建寄存器响应
             */
            static lldbprotobuf::RegistersResponse CreateRegistersResponse(
                bool success,
                const std::vector<lldbprotobuf::Register> &registers = {},
                const std::string &error_message = "");

            /**
             * @brief 创建寄存器组响应
             */
            static lldbprotobuf::RegisterGroupsResponse CreateRegisterGroupsResponse(
                bool success,
                const std::vector<lldbprotobuf::RegisterGroup> &register_groups = {},
                const std::string &error_message = "");

            static lldbprotobuf::AddBreakpointResponse CreateAddBreakpointResponse(
                bool success,
                BreakpointType breakpoint_type,
                const lldbprotobuf::Breakpoint &breakpoint,
                const std::vector<lldbprotobuf::BreakpointLocation> &locations,
                const std::string &error_message = "");

            // ========================================================================
            // 断点类型创建方法
            // ========================================================================

            /**
             * @brief 创建行断点响应
             */
            static lldbprotobuf::AddBreakpointResponse CreateLineBreakpointResponse(
                bool success,
                const lldbprotobuf::Breakpoint &breakpoint,
                const std::vector<lldbprotobuf::BreakpointLocation> &locations,
                const std::string &error_message = "");

            /**
             * @brief 创建地址断点响应
             */
            static lldbprotobuf::AddBreakpointResponse CreateAddressBreakpointResponse(
                bool success,
                const lldbprotobuf::Breakpoint &breakpoint,
                const std::vector<lldbprotobuf::BreakpointLocation> &locations,
                const std::string &error_message = "");

            /**
             * @brief 创建函数断点响应
             */
            static lldbprotobuf::AddBreakpointResponse CreateFunctionBreakpointResponse(
                bool success,
                const lldbprotobuf::Breakpoint &breakpoint,
                const std::vector<lldbprotobuf::BreakpointLocation> &locations,
                const std::string &error_message = "");

            /**
             * @brief 创建观察点响应
             */
            static lldbprotobuf::AddBreakpointResponse CreateWatchpointResponse(
                bool success,
                int64_t watchpoint_id,
                const std::string &error_message = "");

            /**
             * @brief 创建符号断点响应
             */
            static lldbprotobuf::AddBreakpointResponse CreateSymbolBreakpointResponse(
                bool success,
                const lldbprotobuf::Breakpoint &breakpoint,
                const std::vector<lldbprotobuf::BreakpointLocation> &locations,
                const std::string &error_message = "");

            // ========================================================================
            // 控制台命令响应创建
            // ========================================================================

            /**
             * @brief 创建执行命令响应
             *
             * @param success 命令是否成功执行
             * @param output 命令的标准输出
             * @param error_output 命令的错误输出
             * @param return_status LLDB 返回状态码 (0-7)
             * @param error_message 错误消息（失败时）
             */
            static lldbprotobuf::ExecuteCommandResponse CreateExecuteCommandResponse(
                bool success,
                const std::string &output = "",
                const std::string &error_output = "",
                int32_t return_status = 0,
                const std::string &error_message = "");
        };
    } // namespace Debugger
} // namespace Cangjie

#endif // CANGJIE_DEBUGGER_PROTO_CONVERTER_H
