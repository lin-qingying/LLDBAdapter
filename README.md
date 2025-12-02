# Cangjie LLDB Debugger

<div align="center">

**High-Performance Debugger Backend for Cangjie Programming Language**

Based on LLDB 15.0.4 | Protocol Buffers Communication | Cross-Platform Support

[![License](https://img.shields.io/badge/license-Apache%202.0%20with%20Runtime%20Library%20Exception-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16.5%2B-blue.svg)](https://cmake.org/)

</div>

---

## 📖 Overview

A debugger backend designed for the **Cangjie programming language**, using **LLDB 15.0.4** as the underlying debugging engine and **Protocol Buffers** for efficient binary communication. The debugger follows a client-server architecture and can integrate with any frontend that supports TCP and Protobuf (such as IDE plugins, editor extensions, etc.).

### 🌟 Core Features

- **Complete Debugging Functionality**: Breakpoint management, execution control, variable inspection, memory operations, disassembly, etc.
- **Efficient Communication Protocol**: Binary serialization using Protocol Buffers for superior performance
- **Cross-Platform Support**: Windows (MSVC/MinGW), Linux (x86_64/ARM64/ARM32), macOS (x86_64/ARM64)
- **Deep LLDB Integration**: Fully leverages the powerful LLDB C++ API
- **Extensible Architecture**: Adaptable to other programming languages using LLVM backend
- **Static Linking**: Standard library and protobuf statically linked to reduce runtime dependencies

### 🔄 Extension to Other LLVM Languages

While this project is designed for Cangjie, it can theoretically support **any language using LLVM as the compiler backend** (such as Rust, Swift, C/C++, Kotlin Native, etc.) thanks to its use of the standard LLDB API.

To adapt to other languages, you need to:
1. **Replace LLDB dependency**: Substitute the current fixed LLDB 15.0.4 library with the LLDB version used by the target language
2. **Adjust symbol resolution**: Modify type conversion logic in `ProtoConverter` according to the target language's debug symbol format
3. **Extend protocol**: Add language-specific message types in `schema/*.proto` if needed

For detailed instructions, see the [Extension Guide](#-extension-guide-for-other-llvm-languages) section.

---

## 🚀 Quick Start

### Prerequisites

| Component | Version Required | Description |
|-----------|------------------|-------------|
| **Compiler** | GCC 10+ / Clang 12+ / MinGW | C++17 standard support |
| **CMake** | 3.16.5+ | Build system |
| **LLDB** | 15.0.4 | Debugging engine (included in `third_party/`) |
| **Protocol Buffers** | 3.x | Automatically built from source |

### Quick Build

```bash
# Clone the repository
git clone <repository-url>
cd cangjie_debugger

# Build using CMake presets (recommended)
cmake --preset windows-default    # Windows with MSVC
# cmake --preset windows-mingw    # Windows with MinGW
# cmake --preset linux-default    # Linux

cmake --build --preset windows-release
```

### Alternative Build Method

```bash
# Traditional CMake build
mkdir build && cd build
cmake ..
cmake --build .

# Or on Unix systems
make
```

### Cross-Compilation for ARM

The project provides toolchain files for cross-compiling from Linux AMD64 to ARM architectures:

#### ARM64 (aarch64)
```bash
# Install cross-compilation toolchain (Ubuntu/Debian)
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Configure and build
mkdir build-arm64 && cd build-arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake ..
cmake --build .
```

#### ARM32 (armhf)
```bash
# Install cross-compilation toolchain (Ubuntu/Debian)
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Configure and build
mkdir build-arm32 && cd build-arm32
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm32.cmake ..
cmake --build .
```

#### Custom Toolchain Paths
If your toolchain is installed in a non-standard location:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_C_COMPILER=/path/to/aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=/path/to/aarch64-linux-gnu-g++ \
      ..
```

#### Using Sysroot
For cross-compilation with specific ARM libraries:
```bash
# Edit the toolchain file and uncomment the CMAKE_SYSROOT lines
# Or pass it via command line:
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-linux-arm64.cmake \
      -DCMAKE_SYSROOT=/path/to/arm64/sysroot \
      ..
```

The main executable will be available at:
- **Windows**: `output/CangJieLLDBAdapter.exe`
- **Linux/macOS**: `output/CangJieLLDBAdapter`
- **ARM64**: `output/CangJieLLDBAdapter` (cross-compiled)
- **ARM32**: `output/CangJieLLDBAdapter` (cross-compiled)

### Running the Debugger

The debugger requires a port number for TCP communication:

```bash
# Start debugger frontend listening on port 8080
output/CangJieLLDBAdapter.exe 8080  # Windows
./output/CangJieLLDBAdapter 8080     # Linux/macOS
```

### Build Instructions by Platform

#### Windows (MinGW)

```bash
# 1. Clone repository with submodules
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. Configure build
cmake -B build -G "Ninja" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-amd64.cmake

# 3. Build project
cmake --build build --config Release

# 4. Executable located at
# output/CangJieLLDBAdapter_windows_amd64.exe
```

#### Linux (x86_64)

```bash
# 1. Clone repository
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build

# 3. Configure build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-amd64.cmake

# 4. Build project
cmake --build build

# 5. Executable located at
# output/CangJieLLDBAdapter_linux_amd64
```

#### macOS (Apple Silicon)

```bash
# 1. Clone repository
git clone --recursive <repository-url>
cd cangjie_debugger

# 2. Install dependencies
brew install cmake ninja llvm

# 3. Configure build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-arm64.cmake

# 4. Build project
cmake --build build

# 5. Executable located at
# output/CangJieLLDBAdapter_macos_arm64
```

---

## ✨ Detailed Features

### 1. Session Management
- Create debug targets (load executable files)
- Launch process (support for arguments, environment variables, working directory)
- Attach to running processes
- Detach and terminate processes

### 2. Execution Control
- Continue execution
- Pause execution (Suspend)
- Step execution
  - Step Into
  - Step Over
  - Step Out
- Run to cursor

### 3. Breakpoint Management
- **Line Breakpoints**: Set at specific lines in source files
- **Address Breakpoints**: Set at memory addresses
- **Function Breakpoints**: Set at function entry points
- **Conditional Breakpoints**: Breakpoints with condition expressions
- **Symbol Breakpoints**: By function name or regex pattern
- **Watchpoints (Data Breakpoints)**: Monitor memory changes (read/write/read-write)
- Breakpoint operations: enable, disable, delete, update

### 4. Variables and Expressions
- Get local variables list
- Get function parameters
- Get global variables
- Get/set variable values
- Expand complex types (structs, arrays, pointers)
- Expression evaluation (in stack frame context)

### 5. Call Stack and Threads
- Get all threads list
- Get thread call stack
- Get stack frame details (function name, source location, PC address)
- Thread state query

### 6. Memory and Registers
- Read memory blocks
- Write memory blocks
- Get register values
- Register group management

### 7. Disassembly
- Disassemble instructions at specified address
- Configure disassembly options (show machine code, symbolicate)

### 8. Asynchronous Event Push
- Process state change events (launch, stop, running, exit, crash)
- Breakpoint hit events
- Module load/unload events
- Process stdout/stderr
- Thread state change events

---

## 📁 Project Structure

```
cangjie_debugger/
├── schema/                     # Protocol Buffers definitions
│   ├── model.proto            # Core data structures
│   ├── request.proto          # Request messages (20+ types)
│   ├── response.proto         # Response messages
│   └── event.proto           # Event/broadcast messages
├── src/                       # Source code
│   ├── client/               # Client-side networking and debugging
│   │   ├── DebuggerClient.cpp # Main debugger client
│   │   └── TcpClient.cpp     # TCP communication layer
│   ├── core/                 # Core debugging functionality
│   │   └── BreakpointManager.cpp # Breakpoint management
│   ├── protocol/             # Protocol handling
│   │   └── ProtoConverter.cpp # Protobuf conversion utilities
│   ├── utils/                # Utility functions
│   │   └── Logger.cpp        # Thread-safe logging system
│   └── main.cpp              # Application entry point
├── include/                   # Header files
│   └── cangjie/debugger/     # Public API headers
├── cmake/                     # CMake modules and presets
├── third_party/              # Dependencies (protobuf, llvm-project)
├── CMakePresets.json         # Build presets for different platforms
└── tests/                    # Test files (planned)
```

## 🔧 Protocol Buffers

### Automatic Generation

This project uses Protocol Buffers v3 with a custom LLDB-focused protocol. Proto files are automatically generated:

1. **During Configuration**: When running `cmake ..`
2. **On Changes**: When `.proto` files are modified
3. **Manual Regeneration**:
   ```bash
   cmake --build . --target regenerate_protoids
   ```

### Generated Files Location

```
build/generated/proto/
├── model.pb.h/.cc
├── request.pb.h/.cc
├── response.pb.h/.cc
└── event.pb.h/.cc
```

## 🎯 Features

- ✅ **Protocol Buffers v3**: Custom protocol optimized for LLDB integration
- ✅ **LLDB Integration**: Dynamic runtime linking with liblldb
- ✅ **Cross-platform Support**: Windows (MSVC/MinGW), Linux, macOS
- ✅ **Automatic Proto Generation**: Seamless development workflow
- ✅ **Event-driven Architecture**: Asynchronous event processing
- ✅ **TCP Communication**: Network-based debugging protocol
- ✅ **Comprehensive Debug Features**: Breakpoints, stepping, variables, memory inspection

## 🏗️ Architecture

The debugger uses a layered architecture:

```
Debug Frontend (IDE/Editor)
    ↓ (TCP + Protocol Buffers)
CangJieLLDBAdapter (Main Executable)
    ↓ (Dynamic Library Loading)
liblldb.dll / liblldb.so / liblldb.dylib
    ↓
Target Cangjie Program Process
```

### Core Components

1. **LLDB Integration Layer** (`src/client/`)
   - `DebuggerClient.cpp`: Main debugger interface handling protocol messages
   - `TcpClient.cpp`: Network communication using protobuf messages
   - Dynamic LLDB library loading for cross-platform compatibility

2. **Protocol Communication Layer** (`src/protocol/`)
   - `ProtoConverter.cpp`: Converts between protobuf messages and LLDB objects
   - Custom protobuf schema with Chinese documentation
   - Efficient binary serialization for network communication

3. **Debug Management Layer** (`src/core/`)
   - `BreakpointManager.cpp`: Manages line, address, function, and watchpoint operations
   - Thread state management and enumeration
   - Variable evaluation and inspection

4. **Utility Layer** (`src/utils/`)
   - `Logger.cpp`: Thread-safe logging system with multiple levels
   - Platform abstraction and error handling

## 🔧 Development Workflows

### Adding New Debug Commands

1. Define message in appropriate `.proto` file (`request.proto`, `response.proto`, or `event.proto`)
2. Add handler in `DebuggerClient::Handle*Request()`
3. Add response sender in `DebuggerClient::Send*Response()`
4. Update `ProtoConverter` for any new data types

### Protocol Buffer Changes

1. Edit `.proto` files in `schema/` directory
2. CMake automatically regenerates during next build configuration
3. Include generated headers from `build/generated/proto/`
4. Test regeneration with `cmake --build . --target regenerate_protoids`

### Build Targets

```bash
# Build main executable only
cmake --build . --target CangJieLLDBAdapter

# Regenerate protobuf files manually
cmake --build . --target regenerate_protoids

# Rebuild protobuf from source
cmake --build . --target rebuild_protobuf
```

## 📡 Communication Protocol

### Message Format

All messages use **Protocol Buffers** serialization and are transmitted over TCP with a **4-byte length prefix + message body** format:

```
┌───────────────┬─────────────────────────┐
│ Length (4B)   │ Protobuf Message Body   │
│ Big-Endian    │ (Serialized)            │
└───────────────┴─────────────────────────┘
```

### Request-Response Pattern

1. **Frontend → Backend**: Send `Request` message (with unique hash)
2. **Backend → Frontend**: Return `Response` message (carrying the same hash)
3. Frontend matches requests and responses via hash

### Event Push

Backend can push `Event` messages to frontend at any time without request:
- `ProcessStateChanged`: Process state changes
- `ProcessOutput`: Process output (stdout/stderr)
- `BreakpointChangedEvent`: Breakpoint state changes
- `ModuleEvent`: Module load/unload

### Protocol Definitions

All protocol definitions are in the `schema/` directory:
- **`model.proto`**: Core data structures (~1500 lines)
- **`request.proto`**: 20+ request types
- **`response.proto`**: Corresponding response messages
- **`event.proto`**: Asynchronous event messages

---

## 🌍 Extension Guide for Other LLVM Languages

### Step 1: Replace LLDB Dependency

The project currently uses **LLDB 15.0.4**, located in `third_party/lib/liblldb_*` files.

#### 1.1 Determine Target Language's LLDB Version

Different languages' LLVM toolchains may use different LLDB versions:
- **Rust**: Typically uses LLVM 17+
- **Swift**: Comes with macOS (version varies with Xcode)
- **C/C++**: Depends on Clang version
- **Kotlin Native**: LLVM 14+

#### 1.2 Replace LLDB Library Files

```bash
# Example: Replace with LLDB 17.0
cd third_party/lib

# Backup current version
mv liblldb_linux_amd64.so liblldb_linux_amd64.so.bak

# Copy new LLDB version
cp /path/to/llvm-17/lib/liblldb.so.17 liblldb_linux_amd64.so

# Or create symbolic link
ln -s /usr/lib/llvm-17/lib/liblldb.so.17 liblldb_linux_amd64.so
```

#### 1.3 Update LLDB Headers

```bash
# Update LLDB headers
cd third_party/llvm-project
rm -rf lldb
cp -r /path/to/llvm-17/include/lldb lldb/include
```

### Step 2: Adjust Symbol Resolution Logic

Different languages' debug symbol formats may vary (e.g., name mangling, type representation). Adjustments needed in `ProtoConverter.cpp`:

#### 2.1 Modify Type Name Resolution

```cpp
// src/protocol/ProtoConverter.cpp
// CreateType() method

// Original (Cangjie language)
if (type_name.find("cangjie::") == 0) {
  // Handle Cangjie-specific types
}

// Modified for target language (e.g., Rust)
if (type_name.find("alloc::") == 0 ||
    type_name.find("core::") == 0) {
  // Handle Rust standard library types
}
```

#### 2.2 Adjust Function Name Resolution

```cpp
// DebuggerClient.cpp
// HandleFramesRequest() method

// Adjust function name display based on target language's naming rules
std::string function_name = sb_frame.GetFunctionName();
// May need demangling or special handling
```

### Step 3: Extend Protocol (Optional)

If the target language has special debugging needs, extend Protocol Buffers definitions:

#### 3.1 Add Language-Specific Messages

In `schema/model.proto`, add:

```protobuf
// Example: Rust-specific ownership information
message RustOwnershipInfo {
  bool is_moved = 1;
  bool is_borrowed = 2;
  string lifetime = 3;
}

message Value {
  // Existing fields...

  // Add Rust-specific field
  optional RustOwnershipInfo rust_info = 100;
}
```

#### 3.2 Regenerate Protobuf Code

```bash
cmake --build build --target regenerate_protos
```

### Step 4: Test and Validate

1. **Compile test program**: Use target language to compile a simple program (with debug symbols)
2. **Start debugger**: Run `CangJieLLDBAdapter`
3. **Connect and test**: Use test script to send debug commands
4. **Verify functionality**: Breakpoints, variable inspection, stepping, etc.

### Example: Adapting to Rust

```bash
# 1. Install Rust toolchain
rustup install stable
rustup component add lldb

# 2. Find Rust's LLDB
RUST_LLDB=$(rustup which lldb)
LLDB_LIB=$(dirname $(dirname $RUST_LLDB))/lib/liblldb.so

# 3. Replace LLDB library
cp $LLDB_LIB third_party/lib/liblldb_linux_amd64.so

# 4. Rebuild
cmake --build build

# 5. Test debugging Rust program
cargo build --example hello
./output/CangJieLLDBAdapter_linux_amd64 8080
# (Send CreateTarget, AddBreakpoint, Launch commands from frontend)
```

---

## 🧪 Testing

Test infrastructure is planned but not yet implemented. When implemented:

```bash
cd build
ctest  # Run all tests

# Individual test components (planned)
./tests/test_protocol_handler
./tests/test_logger
./tests/test_tcp_client
./tests/test_breakpoint_manager
./tests/test_proto_converter
```

## 🐛 Troubleshooting

### Build Issues

**Protobuf Generation Errors**:
```bash
# Check protoc availability
protoc --version

# Force regeneration
cmake --build . --target regenerate_protoids
```

**LLDB Not Found**:
- Ensure LLDB is installed on your system
- Check `third_party/` directory for liblldb files
- Windows users should have `liblldb.dll` in `third_party/`

**Compilation Errors**:
- Verify C++20 compiler support
- Check CMake version (3.16.5+)
- Ensure all dependencies are in `third_party/`

### Runtime Issues

**Connection Failed**:
- Verify port number is valid (1-65535)
- Check firewall settings
- Ensure no other process is using the same port

**LLDB Initialization Failed**:
- Check LLDB installation
- Verify liblldb library compatibility
- Enable debug logging for detailed error information

### Debug Logging

Enable verbose logging to troubleshoot issues:

```cpp
// In main.cpp, modify log level
Cangjie::Debugger::Logger::Initialize("cangjie_debugger.log",
                                     Cangjie::Debugger::LogLevel::DEBUG, true);
```

## 📚 API Documentation

The project uses a custom Protocol Buffers schema with detailed Chinese comments. Key message types:

- **Request Messages** (`request.proto`): 20+ request types including CreateTarget, Launch, Attach, Continue, StepInto, StepOver, StepOut, AddBreakpoint, Variables, Evaluate, etc.
- **Data Models** (`model.proto`): Thread, Frame, Variable, Breakpoint, ProcessInfo, SourceLocation
- **Events** (`event.proto`): ProcessStopped, ProcessExited, ModuleLoaded, BreakpointChanged, etc.

## 🤝 Contributing

We welcome community contributions! Please follow these steps:

1. Fork the project repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Create a Pull Request

### Development Guidelines

- Follow the existing code style and naming conventions
- Add appropriate logging using the `LOG_INFO()`, `LOG_ERROR()`, etc. macros
- Ensure all LLDB operations check `IsValid()` before use
- Test across platforms when making platform-specific changes
- Update protobuf schemas and regenerate when adding new protocol messages

## 📄 License

This project is licensed under the Apache License 2.0 with Runtime Library Exception. See [LICENSE](LICENSE) file for details.

## 🔗 Related Links

- [LLDB Official Documentation](https://lldb.llvm.org/)
- [Protocol Buffers Documentation](https://developers.google.com/protocol-buffers)
- [Cangjie Language Website](https://cangjie-lang.cn/)
- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

---

**Note**: This is the LLDB frontend for the Cangjie debugger. It communicates via TCP using a custom protobuf protocol and requires a separate debug frontend or IDE integration for a complete debugging experience.