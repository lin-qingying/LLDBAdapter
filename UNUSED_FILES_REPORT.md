# Unused Files and Proto Structures Report

## 1. Unused Source Files

### Adapter (Not used in current implementation)
- `src/adapter/DebugAdapter.cpp`
- `src/adapter/DebugAdapterMain.cpp`

### Client (Old implementation, replaced by TcpClient and DebuggerClient)
- `src/client/CommandInterpreter.cpp`
- `src/client/DebugClient.cpp`
- `src/client/NetworkClient.cpp`

### Server (Not used - we are implementing a client, not a server)
- `src/server/ClientConnection.cpp`
- `src/server/DebugServer.cpp`
- `src/server/NetworkListener.cpp`
- `src/server/SessionManager.cpp`

### Protocol (Old implementation, replaced by ProtoConverter)
- `src/protocol/DebugMessage.cpp`
- `src/protocol/MessageDeserializer.cpp`
- `src/protocol/MessageSerializer.cpp`
- `src/protocol/ProtocolHandler.cpp`

### Core (Commented out in CMakeLists.txt)
- `src/core/DebugServer.cpp` (duplicate with server/)
- `src/core/CangjieDebugger.cpp`

### LLDB (Commented out in CMakeLists.txt - API compatibility issues)
- `src/lldb/LLDBWrapper.cpp`
- `src/lldb/LLDBDebugger.cpp`
- `src/lldb/LLDBProcess.cpp`
- `src/lldb/LLDBThread.cpp`
- `src/lldb/LLDBBreakpoint.cpp`
- `src/lldb/LLDBVariable.cpp`

### Utils (Commented out in CMakeLists.txt)
- `src/utils/StringUtils.cpp`
- `src/utils/FileUtils.cpp`
- `src/utils/PlatformUtils.cpp`
- `src/utils/ThreadPool.cpp`

## 2. Currently Used Files

### Main
- `src/main.cpp` ✓

### Client (New implementation)
- `src/client/TcpClient.cpp` ✓
- `src/client/DebuggerClient.cpp` ✓

### Protocol
- `src/protocol/ProtoConverter.cpp` ✓

### Core
- `src/core/BreakpointManager.cpp` ✓
- `src/core/VariableInspector.cpp` ✓
- `src/core/ThreadManager.cpp` ✓
- `src/core/ModuleManager.cpp` ✓
- `src/core/DebugSession.cpp` ✓

### Utils
- `src/utils/Logger.cpp` ✓

## 3. Proto Structures - Implemented in ProtoConverter

### Requests (Implemented)
- CreateTargetRequest ✓
- LaunchRequest ✓
- ContinueRequest ✓
- StepIntoRequest ✓
- StepOverRequest ✓
- StepOutRequest ✓
- AddBreakpointRequest ✓
- RemoveBreakpointRequest ✓
- GetThreadsRequest ✓
- GetFramesRequest ✓
- EvaluateExpressionRequest ✓

### Responses (Implemented)
- ResponseStatus ✓
- LaunchResponse ✓
- GetThreadsResponse ✓
- GetFramesResponse ✓
- AddBreakpointResponse ✓

### Events (Implemented)
- InitializedEvent ✓
- ProcessInterruptedEvent ✓
- ProcessRunningEvent ✓
- ProcessExitedEvent ✓
- ProcessOutputEvent ✓
- BreakpointAddedEvent ✓
- BreakpointRemovedEvent ✓
- ModulesLoadedEvent ✓
- LogMessageEvent ✓

## 4. Proto Structures - NOT Implemented (Available for future use)

### Process Management Requests
- AttachRequest
- AttachByNameRequest
- RemoteLaunchRequest
- LoadCoreRequest
- DetachRequest (used in DebuggerClient but not in ProtoConverter)
- KillRequest (used in DebuggerClient but not in ProtoConverter)
- ExitRequest (used in DebuggerClient but not in ProtoConverter)

### Execution Control Requests
- SuspendRequest (used in DebuggerClient but not in ProtoConverter)
- StepScriptedRequest
- JumpToLineRequest
- JumpToAddressRequest

### Breakpoint/Watchpoint Requests
- AddWatchpointRequest
- RemoveWatchpointRequest

### Variable/Value Requests
- GetVariablesRequest (used in DebuggerClient but not in ProtoConverter)
- GetValueChildrenRequest
- GetArraySliceRequest
- GetValueDataRequest
- GetValueDescriptionRequest
- GetChildrenCountRequest
- GetValueAddressRequest
- SetValueFilteringPolicyRequest

### Memory/Disassembly Requests
- DisassembleRequest
- DisassembleUntilPivotRequest
- DumpSectionsRequest
- DumpMemoryRequest
- WriteMemoryRequest
- GetContextInfoRequest

### Register Requests
- GetRegistersRequest
- GetRegisterSetsRequest
- GetArchitectureRequest

### Console/Command Requests
- DispatchInputRequest
- HandleConsoleCommandRequest
- HandleCompletionRequest
- ResizeConsoleRequest

### Platform/Remote Requests
- ConnectPlatformRequest
- ConnectProcessRequest

### Thread Control Requests
- FreezeThreadRequest
- UnfreezeThreadRequest

### Signal/Shell Requests
- HandleSignalRequest
- ExecuteShellCommandRequest
- CancelSymbolsDownloadRequest

## 5. Recommendations

### Files to Delete (Safe to remove)
1. **Old client implementation**: `src/client/{CommandInterpreter,DebugClient,NetworkClient}.cpp`
2. **Old protocol implementation**: `src/protocol/{DebugMessage,MessageDeserializer,MessageSerializer,ProtocolHandler}.cpp`
3. **Server files** (if building client only): `src/server/*.cpp`
4. **Adapter files** (if not using DAP): `src/adapter/*.cpp`

### Files to Keep (May be needed later)
1. **LLDB wrappers**: Keep for future LLDB integration
2. **Utils**: Keep for potential future use
3. **Core managers**: Currently used

### Proto Structures to Implement Next (Priority)
1. **Watchpoints**: AddWatchpointRequest, RemoveWatchpointRequest
2. **Variable inspection**: GetValueChildrenRequest, GetValueDataRequest
3. **Memory operations**: DumpMemoryRequest, WriteMemoryRequest
4. **Console commands**: HandleConsoleCommandRequest
5. **Attach operations**: AttachRequest, AttachByNameRequest
