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

    bool DebuggerClient::SendRunToCursorResponse(bool success, uint64_t temp_breakpoint_id,
                                                  const std::string &method_used,
                                                  const std::string &error_message,
                                                  const std::optional<uint64_t> hash) const {
        auto run_to_cursor_resp = ProtoConverter::CreateRunToCursorResponse(
            success, temp_breakpoint_id, method_used, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_run_to_cursor() = run_to_cursor_resp;

        LOG_INFO("Sending RunToCursor response: success=" + std::to_string(success) +
                 ", method=" + method_used);
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

    bool DebuggerClient::SendExecuteCommandResponse(bool success,
                                                     const std::string &output,
                                                     const std::string &error_output,
                                                     int32_t return_status,
                                                     const std::string &error_message,
                                                     const std::optional<uint64_t> hash) const {
        auto execute_cmd_resp = ProtoConverter::CreateExecuteCommandResponse(
            success,
            output,
            error_output,
            return_status,
            error_message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_execute_command() = execute_cmd_resp;

        LOG_INFO("Sending ExecuteCommand response: success=" + std::to_string(success));
        return tcp_client_.SendProtoMessage(response);
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

    bool DebuggerClient::SendDisassembleResponse(bool success,
                                                 const std::vector<lldbprotobuf::DisassembleInstruction> &instructions,
                                                 uint32_t bytes_disassembled,
                                                 bool alignment_verified,
                                                 uint64_t actual_end_address,
                                                 const std::string &error_message,
                                                 const std::optional<uint64_t> hash) const {
        auto disassemble_resp = ProtoConverter::CreateDisassembleResponse(
            success,
            instructions,
            bytes_disassembled,
            alignment_verified,
            actual_end_address,
            error_message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_disassemble() = disassemble_resp;

        LOG_INFO("Sending Disassemble response: success=" + std::to_string(success) +
            ", instructions=" + std::to_string(instructions.size()) +
            ", bytes=" + std::to_string(bytes_disassembled) +
            ", alignment_verified=" + std::to_string(alignment_verified));

        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendGetFunctionInfoResponse(bool success,
                                                     const std::vector<lldbprotobuf::FunctionInfo> &functions,
                                                     const std::string &error_message,
                                                     const std::optional<uint64_t> hash) const {
        auto function_info_resp = ProtoConverter::CreateGetFunctionInfoResponse(
            success,
            functions,
            error_message
        );

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }

        *response.mutable_get_function_info() = function_info_resp;

        LOG_INFO("Sending GetFunctionInfo response: success=" + std::to_string(success) +
            ", functions=" + std::to_string(functions.size()));

        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendRegistersResponse(bool success,
                                              const std::vector<lldbprotobuf::Register> &registers,
                                              const std::string &error_message,
                                              const std::optional<uint64_t> hash) const {
        auto registers_resp = ProtoConverter::CreateRegistersResponse(success, registers, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_registers() = registers_resp;

        LOG_INFO("Sending Registers response: success=" + std::to_string(success) +
                ", register_count=" + std::to_string(registers.size()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendRegisterGroupsResponse(bool success,
                                                     const std::vector<lldbprotobuf::RegisterGroup> &register_groups,
                                                     const std::string &error_message,
                                                     const std::optional<uint64_t> hash) const {
        auto register_groups_resp = ProtoConverter::CreateRegisterGroupsResponse(success, register_groups, error_message);

        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_register_groups() = register_groups_resp;

        LOG_INFO("Sending RegisterGroups response: success=" + std::to_string(success) +
                ", group_count=" + std::to_string(register_groups.size()));
        return tcp_client_.SendProtoMessage(response);
    }

    bool DebuggerClient::SendCommandCompletionResponse(bool success,
                                                        const std::vector<std::string> &completions,
                                                        const std::string &common_prefix,
                                                        uint32_t completion_start,
                                                        bool has_more,
                                                        const std::string &error_message,
                                                        const std::optional<uint64_t> hash) const {
        // 使用 ProtoConverter 创建响应消息
        auto completion_resp = ProtoConverter::CreateCommandCompletionResponse(
            success,
            completions,
            common_prefix,
            completion_start,
            has_more,
            error_message
        );

        // 创建完整响应
        lldbprotobuf::Response response;
        if (hash.has_value()) {
            *response.mutable_hash() = CreateHashId(hash.value());
        }
        *response.mutable_command_completion() = completion_resp;

        LOG_INFO("Sending CommandCompletion response: success=" + std::to_string(success) +
            ", completions=" + std::to_string(completions.size()) +
            ", common_prefix='" + common_prefix + "'" +
            ", completion_start=" + std::to_string(completion_start) +
            ", has_more=" + std::to_string(has_more));

        return tcp_client_.SendProtoMessage(response);
    }

} // namespace Cangjie::Debugger