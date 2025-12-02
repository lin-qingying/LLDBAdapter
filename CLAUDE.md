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
cmake --build . --target CangJieLLDBAdapter

# Regenerate protobuf files manually
cmake --build . --target regenerate_protoids

# Rebuild protobuf from source
cmake --build . --target rebuild_protobuf
```

### Running the Debugger
```bash
# The main executable requires a port number for TCP communication
./output/CangJieLLDBAdapter 8080

# Example: Start debugger frontend listening on port 8080
output/CangJieLLDBAdapter.exe 8080  # Windows
./output/CangJieLLDBAdapter 8080     # Linux/macOS
```

### Testing (Planned)
```bash
cd build
ctest  # Run all tests (when test files are implemented)

# Individual test components (planned):
./tests/test_protocol_handler
./tests/test_logger
./tests/test_tcp_client
./tests/test_breakpoint_manager
./tests/test_proto_converter
```
**Note**: Test infrastructure is planned but not yet implemented.

### Clean Build
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

### Cross-Compilation for ARM Architectures

The project provides toolchain files for cross-compiling from Linux AMD64 to ARM.

#### ARM64 (aarch64) Cross-Compilation - 自动化脚本（推荐）

使用提供的自动化脚本进行两阶段交叉编译（先在宿主机编译 protobuf 和 protoc，然后使用宿主机的 protoc 编译 proto 文件，最后交叉编译项目）：

```bash
# 安装交叉编译工具链（Ubuntu/Debian）
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 赋予脚本执行权限
chmod +x scripts/linux-amd64-to-arm64.sh

# 运行自动化交叉编译脚本
./scripts/linux-amd64-to-arm64.sh

# 指定构建类型（默认为 Release）
BUILD_TYPE=Debug ./scripts/linux-amd64-to-arm64.sh
```

脚本执行的三个阶段：
1. **阶段 1**：在宿主机（Linux AMD64）使用系统 GCC 编译 protobuf 和 protoc 工具
2. **阶段 2**：使用宿主机的 protoc 生成 C++ 代码（`.pb.cc` 和 `.pb.h` 文件）
3. **阶段 3**：使用 ARM64 交叉编译器编译整个项目

**优点**：
- 完全自动化，一键完成所有步骤
- 避免交叉编译 protoc 的复杂性
- 支持增量编译，已编译的部分会被跳过
- 彩色输出，清晰显示编译进度
- 自动验证生成的可执行文件架构
- 自动使用系统 GCC (`/usr/bin/gcc`) 编译宿主机 protobuf
- 支持使用 Ninja 构建系统加速编译

**生成的构建目录**：
- `build-linux-amd64/`: 宿主机构建目录（包含 protoc）
- `build-linux-arm64/`: 目标平台构建目录
- `output/`: 最终可执行文件输出目录

#### ARM64 (aarch64) Cross-Compilation - 手动步骤

如果需要手动控制每个步骤，可以使用以下方法：

```bash
# 1. 安装交叉编译工具链（Ubuntu/Debian）
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 2. 先在宿主机编译 protobuf 和 protoc
mkdir build-linux-amd64 && cd build-linux-amd64
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=/usr/bin/gcc \
      -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
      ..
cmake --build . --config Release --parallel
cd ..

# 3. 使用宿主机的 protoc 进行交叉编译
mkdir build-linux-arm64 && cd build-linux-arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/linux-arm64.cmake \
      -DPROTOC_HOST=../build-linux-amd64/host/protobuf-install/bin/protoc \
      ..
cmake --build .
```

#### ARM32 (armhf) Cross-Compilation
```bash
# Install toolchain (Ubuntu/Debian)
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Configure and build
mkdir build-arm32 && cd build-arm32
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm32.cmake ..
cmake --build .
```

#### Custom Cross-Compilation Settings
```bash
# Custom compiler paths
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_C_COMPILER=/custom/path/aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=/custom/path/aarch64-linux-gnu-g++ \
      ..

# Using sysroot for ARM libraries
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_SYSROOT=/path/to/arm64/sysroot \
      ..
```

**Toolchain Files**:
- `cmake/toolchain-linux-arm64.cmake`: Linux AMD64 → ARM64 (aarch64)
- `cmake/toolchain-linux-arm32.cmake`: Linux AMD64 → ARM32 (armhf)

**Note**: When cross-compiling, ensure all dependencies (protobuf, LLDB) are available for the target architecture.

## Project Architecture

This is a **Cangjie language debugger** built using a layered architecture with LLDB as the debugging backend and Protocol Buffers for communication. The project uses a custom protobuf-based protocol designed specifically for LLDB integration.

### Core Components

1. **LLDB Integration Layer** (`src/client/DebuggerClient.cpp`, `src/client/TcpClient.cpp`)
   - Dynamically links with LLDB libraries at runtime
   - Provides C++ wrappers around LLDB C++ API
   - Handles platform-specific LLDB integration (Windows/Linux/macOS)
   - Main executable: `CangJieLLDBAdapter`

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

**Dynamic LLDB Linking**:
- Runtime dynamic linking with `liblldb.dll/.so/.dylib`
- Platform-specific library linking
- Consistent LLDB API usage across platforms

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

- **C++20**: Required standard with some C++17 components (CMake sets this)
- **LLVM/LLDB**: Uses headers from `third_party/llvm-project/` or dynamic linking
- **Protocol Buffers**: Built from source in `third_party/protobuf/` with automatic dependency resolution
- **Threading**: Uses `std::thread` and C++ synchronization primitives
- **CMake 3.16.5+**: For modern CMake features and presets
- **Platform Libraries**: Windows requires dbghelp, psapi, wsock32, ws2_32

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
   - Use LLDB headers from `third_party/llvm-project/lldb/include/`
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
- **Main executable**: `output/CangJieLLDBAdapter.exe` (Windows) or `output/CangJieLLDBAdapter` (Unix)
- **Generated protobuf files**: `build/generated/proto/`
- **Third-party libraries**: `third_party/` contains LLDB and protobuf dependencies
- **Required DLLs**: Automatically copied to `output/` on Windows

### Development Notes

- The project uses dynamic LLDB linking for consistent cross-platform support
- Protocol Buffers messages are automatically regenerated when `.proto` files change
- Logging is available via `LOG_INFO()`, `LOG_ERROR()`, etc. macros from `Logger.h`
- All LLDB operations should check `IsValid()` before use
- The debugger communicates via TCP with debug frontends using the custom protobuf protocol
- Event-driven architecture with dedicated event monitoring thread
- Comprehensive error handling with detailed status reporting

### Testing Strategy

**Current Status**: Test infrastructure is planned but not yet implemented. The `tests/` directory exists but contains no test files.

**Planned Unit Tests** (`tests/`):
- `test_protocol_handler.cpp`: Protocol Buffers communication tests
- `test_logger.cpp`: Logging system functionality
- `test_tcp_client.cpp`: Network communication tests
- `test_breakpoint_manager.cpp`: Breakpoint management tests
- `test_proto_converter.cpp`: Protocol conversion tests

**When implemented**, run with: `cmake --build . && ctest` or individual test execution.

### Performance Considerations

- Efficient protobuf serialization for high-frequency debugging operations
- Minimal memory copying in message handling
- Asynchronous event processing to avoid blocking main debug loop
- Smart pointer usage for LLDB object lifecycle management
- Optional compression support for large data transfers