# Cangjie Debugger

A high-performance debugger for the Cangjie programming language, built with LLDB as the debugging backend and Protocol Buffers for efficient communication.

## 🚀 Quick Start

### Prerequisites

- **Compiler**: C++20 compatible (GCC 10+, Clang 12+, MSVC 2022+)
- **CMake**: Version 3.16.5 or higher
- **LLVM/LLDB**: For debugging backend (automatically handled)
- **Protocol Buffers**: v3 (built from source automatically)

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

The main executable will be available at:
- **Windows**: `output/CangJieLLDBFrontend.exe`
- **Linux/macOS**: `output/CangJieLLDBFrontend`

### Running the Debugger

The debugger requires a port number for TCP communication:

```bash
# Start debugger frontend listening on port 8080
output/CangJieLLDBFrontend.exe 8080  # Windows
./output/CangJieLLDBFrontend 8080     # Linux/macOS
```

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
CangJieLLDBFrontend (Main Executable)
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
cmake --build . --target CangJieLLDBFrontend

# Regenerate protobuf files manually
cmake --build . --target regenerate_protoids

# Rebuild protobuf from source
cmake --build . --target rebuild_protobuf
```

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