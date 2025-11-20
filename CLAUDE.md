# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Standard Build
```bash
mkdir build && cd build
cmake ..
make  # or "cmake --build ." on Windows
```

### With CMake Presets (Recommended)
```bash
# List available presets
cmake --list-presets

# Windows with MSVC
cmake --preset windows-default
cmake --build --preset windows-release

# Windows with MinGW
cmake --preset windows-mingw
cmake --build --preset windows-release

# Linux
cmake --preset linux-default
cmake --build --preset linux-release
```

### Using vcpkg for Dependencies
```bash
# Set VCPKG_ROOT environment variable first
export VCPKG_ROOT=/path/to/vcpkg

# Build with vcpkg toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ..
cmake --build .
```

### Build Specific Components
```bash
# Build main executable only
cmake --build . --target CangJieLLDBFrontend

# Regenerate protobuf files manually
cmake --build . --target regenerate_protoids

# Rebuild protobuf from source
cmake --build . --target rebuild_protobuf
```

### Testing
```bash
cd build
ctest  # or run tests directly
./tests/test_protocol_handler
./tests/test_logger
./tests/test_tcp_client
./tests/test_breakpoint_manager
./tests/test_proto_converter
```

### Clean Build
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

## Project Architecture

This is a **Cangjie language debugger** built using a layered architecture with LLDB as the debugging backend and Protocol Buffers for communication. The project uses a custom protobuf-based protocol designed specifically for LLDB integration.

### Core Components

1. **LLDB Integration Layer** (`src/client/DebuggerClient.cpp`, `src/client/TcpClient.cpp`)
   - Dynamically loads LLDB without compile-time dependencies (`USE_DYNAMIC_LLDB`)
   - Provides C++ wrappers around LLDB C++ API
   - Handles platform-specific LLDB integration (Windows/Linux/macOS)
   - Main executable: `CangJieLLDBFrontend`

2. **Protocol Communication Layer** (`src/protocol/ProtoConverter.cpp`, `schema/`)
   - Uses custom Protocol Buffers v3 schema optimized for LLDB
   - Defines messages in `schema/*.proto` files:
     - `model.proto`: Core data structures (Value, Thread, StackFrame, Breakpoint)
     - `request.proto`: Request messages (20+ request types)
     - `response.proto`: Response messages
     - `event.proto`: Event/broadcast messages
   - Auto-generates C++ code in `build/generated/proto/`
   - Handles binary serialization and network communication

3. **Debug Management Layer** (`src/core/`)
   - `BreakpointManager.cpp`: Line, address, function, and watchpoint operations
   - `ThreadManager.cpp`: Thread state management and enumeration
   - `VariableInspector.cpp`: Variable evaluation and inspection
   - `ModuleManager.cpp`: Module loading and symbol management

4. **Utility Layer** (`src/utils/`)
   - `Logger.cpp`: Thread-safe logging system with multiple levels
   - Error handling and platform abstraction

### Key Classes

- `DebuggerClient`: Main debugger interface handling protocol messages and LLDB integration
- `TcpClient`: Network communication with debug frontend using protobuf messages
- `ProtoConverter`: Converts between protobuf messages and LLDB objects
- `Logger`: Thread-safe logging system with file output support

### Protocol Buffer Schema Design

The project uses a custom LLDB-focused protocol with Chinese comments and detailed API mappings:

**Core Message Types:**
- **Request Messages** (`request.proto`): 20+ request types including CreateTarget, Launch, Attach, Continue, StepInto, StepOver, StepOut, AddBreakpoint, Variables, Evaluate, etc.
- **Data Models** (`model.proto`): Thread, Frame, Variable, Breakpoint, ProcessInfo, SourceLocation
- **Stop Reasons**: Comprehensive enum covering trace, breakpoint, watchpoint, signal, exception, exec, plan_complete, thread_exiting

**Key Features:**
- Stable uint64 IDs for all debug objects (target/process/thread/breakpoint)
- Detailed LLDB API mappings in comments
- Chinese documentation for all fields
- Support for conditional breakpoints, watchpoints, symbol breakpoints
- Expression evaluation with frame context
- Memory reading and register inspection

**Important**: When modifying `.proto` files, CMake automatically regenerates the C++ code during configuration. Use `cmake --build . --target regenerate_protoids` for manual regeneration.

### LLDB API Integration

The debugger uses LLDB's C++ API with dynamic loading patterns:

**Dynamic Loading (`USE_DYNAMIC_LLDB`)**:
- Runtime loading of `liblldb.dll/.so/.dylib` without compile-time dependencies
- Platform-specific library search and loading
- Fallback to system-installed LLDB

**LLDB Object Management**:
- `lldb::SBDebugger`, `lldb::SBTarget`, `lldb::SBProcess`, `lldb::SBThread`
- Stable ID mapping between protocol and LLDB objects
- Event-driven architecture with dedicated event thread

**Key Patterns**:
- Use `target_.DeleteWatchpoint(id)` instead of `RemoveWatchpoint`
- Expression evaluation: `frame.EvaluateExpression(expr, lldb::eNoDynamicValues)`
- Process access through `process_` member with proper initialization checks
- All LLDB operations should check `IsValid()` before use

### Critical Build Dependencies

- **C++20**: Required standard (CMake sets this)
- **LLVM/LLDB**: Uses headers from `third_party/llvm-project/` or dynamic linking
- **Protocol Buffers**: Built from source in `third_party/protobuf/` with automatic dependency resolution
- **Threading**: Uses `std::thread` and C++ synchronization primitives
- **CMake 3.16.5+**: For modern CMake features and presets

### Cross-Platform Considerations

- **Windows**:
  - Supports both MSVC and MinGW toolchains
  - Static linking for libstdc++ and libgcc (MinGW)
  - Dynamic loading of `liblldb.dll` and dependencies
  - Copies required DLLs to output directory automatically
- **Linux**: Runtime dynamic loading of `liblldb.so`
- **macOS**: Runtime dynamic loading of `liblldb.dylib`
- **Paths**: Uses CMake's platform-agnostic path handling

### Common Development Workflows

1. **Adding New Debug Commands**:
   - Define message in appropriate `.proto` file (`request.proto`, `response.proto`, or `event.proto`)
   - Add handler in `DebuggerClient::Handle*Request()`
   - Add response sender in `DebuggerClient::Send*Response()`
   - Update `ProtoConverter` for any new data types

2. **Protocol Buffer Changes**:
   - Edit `.proto` files in `schema/` directory
   - CMake automatically regenerates during next build configuration
   - Include generated headers from `build/generated/proto/`
   - Test regeneration with `cmake --build . --target regenerate_protoids`

3. **LLDB Integration**:
   - Check `USE_DYNAMIC_LLDB` compile definition for dynamic loading mode
   - Use LLDB headers from `third_party/llvm-project/lldb/include/` when available
   - Test across platforms due to LLDB API differences
   - Ensure proper ID mapping between protocol and LLDB objects

4. **Testing**:
   - Unit tests for each component in `tests/` directory
   - Protocol handler tests for message serialization
   - LLDB integration tests require actual debug targets
   - Use `ctest` for comprehensive test execution

### Build System Details

**CMake Configuration**:
- Modern CMake 3.16.5+ with preset support
- Automatic protobuf building from source with fallback to pre-built
- Dynamic LLDB loading with platform detection
- Comprehensive DLL copying for Windows deployment
- Export of compile commands for IDE integration

**Output Structure**:
- **Main executable**: `output/CangJieLLDBFrontend.exe` (Windows) or `output/CangJieLLDBFrontend` (Unix)
- **Generated protobuf files**: `build/generated/proto/`
- **Third-party libraries**: `third_party/` contains LLDB and protobuf dependencies
- **Required DLLs**: Automatically copied to `output/` on Windows

### Development Notes

- The project supports both static and dynamic LLDB linking via `USE_DYNAMIC_LLDB`
- Protocol Buffers messages are automatically regenerated when `.proto` files change
- Logging is available via `LOG_INFO()`, `LOG_ERROR()`, etc. macros from `Logger.h`
- All LLDB operations should check `IsValid()` before use
- The debugger communicates via TCP with debug frontends using the custom protobuf protocol
- Event-driven architecture with dedicated event monitoring thread
- Comprehensive error handling with detailed status reporting

### Testing Strategy

**Unit Tests** (`tests/`):
- `test_protocol_handler.cpp`: Protocol Buffers communication tests
- `test_logger.cpp`: Logging system functionality
- `test_tcp_client.cpp`: Network communication tests
- `test_breakpoint_manager.cpp`: Breakpoint management tests
- `test_proto_converter.cpp`: Protocol conversion tests

Run with: `cmake --build . && ctest` or individual test execution.

### Performance Considerations

- Efficient protobuf serialization for high-frequency debugging operations
- Minimal memory copying in message handling
- Asynchronous event processing to avoid blocking main debug loop
- Smart pointer usage for LLDB object lifecycle management
- Optional compression support for large data transfers