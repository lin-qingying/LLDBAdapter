# Cangjie Debugger Build Instructions

This guide provides detailed instructions for building the Cangjie debugger on different platforms.

## Prerequisites

### System Requirements

- **Operating Systems**: Windows 10+, Ubuntu 18.04+, CentOS 7+, macOS 10.15+
- **Compiler**:
  - Windows: Visual Studio 2019+ or MinGW-w64 8.0+
  - Linux: GCC 9+ or Clang 10+
  - macOS: Xcode 12+ (Clang)
- **RAM**: Minimum 4GB, recommended 8GB+
- **Disk Space**: Minimum 2GB free space

### Required Dependencies

#### Core Dependencies
- **CMake** 3.16.5 or later
- **Protocol Buffers** 3.15 or later
- **Zlib** 1.2.11 or later (for compression support)

#### LLDB Dependencies
- **Windows**: liblldb.dll and related libraries
- **Linux**: liblldb development packages
- **macOS**: Xcode Command Line Tools (includes LLDB)

#### Optional Dependencies
- **Doxygen** (for documentation generation)
- **Google Test** (for unit testing)

## Platform-Specific Setup

### Windows

#### 1. Install Visual Studio
```powershell
# Download and install Visual Studio 2019 or later
# Make sure to install "Desktop development with C++" workload
```

#### 2. Install CMake
Download and install CMake from https://cmake.org/download/

#### 3. Install Protocol Buffers
```powershell
# Using vcpkg (recommended)
vcpkg install protobuf:x64-windows

# Or download pre-built binaries from GitHub releases
# https://github.com/protocolbuffers/protobuf/releases
```

#### 4. Install LLDB
```powershell
# Option 1: Install LLVM (includes LLDB)
# Download from https://releases.llvm.org/

# Option 2: Use vcpkg
vcpkg install llvm:x64-windows

# Copy liblldb.dll and related libraries to third_party/ directory
mkdir third_party
copy "C:\Program Files\LLVM\bin\liblldb.dll" third_party\
copy "C:\Program Files\LLVM\bin\libclang.dll" third_party\
```

#### 5. Build using Python Script
```powershell
# Navigate to project directory
cd D:\code\cangjie\workspace\cangjie_debugger

# Build
python scripts\build.py --build-type Release --enable-tests --run-tests
```

### Linux (Ubuntu/Debian)

#### 1. Install Development Tools
```bash
sudo apt update
sudo apt install -y build-essential cmake git
```

#### 2. Install Protocol Buffers
```bash
# Ubuntu 20.04+ or Debian 11+
sudo apt install -y libprotobuf-dev protobuf-compiler

# For older versions, build from source:
git clone https://github.com/protocolbuffers/protobuf.git
cd protobuf
git submodule update --init --recursive
./autogen.sh
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### 3. Install LLDB
```bash
# Ubuntu 20.04+
sudo apt install -y lldb-14 liblldb-14-dev

# For other versions, install LLVM package
sudo apt install -y llvm-14 lldb-14 clang-14
```

#### 4. Install Optional Dependencies
```bash
# For testing
sudo apt install -y libgtest-dev libgmock-dev

# For documentation
sudo apt install -y doxygen
```

#### 5. Build using Python Script
```bash
# Navigate to project directory
cd /path/to/cangjie_debugger

# Build
python3 scripts/build.py --build-type Release --enable-tests --run-tests
```

### macOS

#### 1. Install Xcode Command Line Tools
```bash
xcode-select --install
```

#### 2. Install Homebrew (if not already installed)
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### 3. Install Dependencies
```bash
brew install cmake protobuf protobuf-c llvm
```

#### 4. Build using Python Script
```bash
# Navigate to project directory
cd /path/to/cangjie_debugger

# Build
python3 scripts/build.py --build-type Release --enable-tests --run-tests
```

## Build Configuration Options

The build script provides various options to customize the build:

### Basic Options
```bash
python scripts/build.py [options]
```

#### Available Options
- `--build-dir DIR`: Build directory (default: build)
- `--build-type TYPE`: Build type (Debug|Release|RelWithDebInfo|MinSizeRel)
- `--clean`: Clean build directory before building
- `--parallel-jobs N`: Number of parallel build jobs (default: auto)
- `--lldb-path PATH`: Path to LLDB library

#### Feature Options
- `--enable-tests`: Enable tests (default: enabled)
- `--disable-tests`: Disable tests
- `--enable-docs`: Enable documentation generation

#### Post-Build Options
- `--run-tests`: Run tests after building
- `--install`: Install after building
- `--install-prefix PATH`: Installation prefix
- `--package TYPE`: Create distribution package (ZIP|TGZ|DEB|RPM)

#### Utility Options
- `--skip-deps-check`: Skip dependency checking

### Example Build Commands

#### Debug Build with Tests
```bash
python scripts/build.py --build-type Debug --enable-tests --run-tests
```

#### Release Build with Installation
```bash
python scripts/build.py --build-type Release --install --install-prefix /usr/local
```

#### Package Creation
```bash
# Create ZIP package
python scripts/build.py --build-type Release --package ZIP

# Create DEB package (Ubuntu/Debian)
python scripts/build.py --build-type Release --package DEB

# Create RPM package (CentOS/RHEL)
python scripts/build.py --build-type Release --package RPM
```

## Manual Build (Without Python Script)

If you prefer to build manually using CMake:

### 1. Generate Protocol Buffers Sources
```bash
# Linux/macOS
protoc --cpp_out=src/protocol schema/*.proto

# Windows
protoc.exe --cpp_out=src\protocol schema\*.proto
```

### 2. Configure CMake
```bash
mkdir build
cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DENABLE_TESTING=ON
```

### 3. Build
```bash
# Linux/macOS
make -j$(nproc)

# Windows (Visual Studio)
cmake --build . --config Release

# Windows (MinGW)
mingw32-make -j$(nproc)
```

### 4. Test
```bash
# Linux/macOS
ctest --output-on-failure

# Windows
ctest --output-on-failure -C Release
```

### 5. Install
```bash
# Linux/macOS
make install

# Windows
cmake --install . --config Release
```

## Troubleshooting

### Common Issues

#### 1. CMake Version Too Old
```
CMake Error: CMake 3.16.5 or higher is required
```
**Solution**: Install a newer version of CMake or use a newer Linux distribution.

#### 2. Protocol Buffers Not Found
```
Could NOT find Protobuf (missing: Protobuf_LIBRARIES)
```
**Solution**: Install Protocol Buffers development package or set CMAKE_PREFIX_PATH.

#### 3. LLDB Library Not Found
```
Could NOT find LLDB (missing: LLDB_LIBRARIES)
```
**Solution**:
- Install LLDB development package
- Or copy liblldb.dll to third_party/ directory (Windows)
- Or set LLDB_PATH environment variable

#### 4. Compilation Errors on Windows
```
error C2146: syntax error: missing ';' before identifier 'uint32_t'
```
**Solution**: Ensure you're using C++17 compatible compiler and have included required headers.

#### 5. Linking Errors
```
undefined reference to `protobuf::...`
```
**Solution**: Ensure Protocol Buffers libraries are properly linked and paths are correct.

### Debug Build Issues

#### 1. Build is Slow
**Solution**: Use `--parallel-jobs` option or reduce build type to `RelWithDebInfo`.

#### 2. Memory Issues During Build
**Solution**: Reduce parallel jobs count or use single-threaded build.

#### 3. Permission Errors
**Solution**: Ensure you have write permissions to build directory and installation prefix.

### Platform-Specific Issues

#### Windows
- **DLL Not Found**: Ensure liblldb.dll and dependencies are in PATH or third_party/ directory
- **Visual Studio Version**: Use VS 2019 or later with C++17 support
- **MinGW Issues**: Use MSYS2 MinGW-w64 8.0 or later

#### Linux
- **Package Names**: Package names vary between distributions (Ubuntu vs CentOS)
- **Library Paths**: May need to set LD_LIBRARY_PATH for LLDB
- **GCC Version**: Use GCC 9+ or Clang 10+

#### macOS
- **Xcode Version**: Ensure Xcode 12+ is installed
- **Command Line Tools**: Run `xcode-select --install`
- **Homebrew**: Use latest Homebrew and updated packages

## Performance Optimization

### Build Performance
```bash
# Use maximum parallel jobs
python scripts/build.py --parallel-jobs $(nproc)

# Use Ninja generator (faster than make)
cmake -G Ninja .. && ninja
```

### Runtime Performance
- Use Release build for production
- Enable compiler optimizations
- Consider RelWithDebInfo for debugging optimized builds

## Continuous Integration

### GitHub Actions Example
```yaml
name: Build and Test
on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]

    steps:
    - uses: actions/checkout@v2

    - name: Setup Dependencies
      run: |
        # Platform-specific setup

    - name: Build
      run: |
        python scripts/build.py --enable-tests --run-tests
```

## Installation and Distribution

### System-wide Installation
```bash
# Install to /usr/local
sudo python scripts/build.py --install --install-prefix /usr/local

# Install to custom prefix
python scripts/build.py --install --install-prefix /opt/cangjie-debugger
```

### Creating Packages
```bash
# Create distribution package
python scripts/build.py --package ZIP

# The package will be created in the build directory
```

### Docker Image
```dockerfile
FROM ubuntu:20.04

RUN apt-get update && apt-get install -y \
    build-essential cmake \
    libprotobuf-dev protobuf-compiler \
    lldb-14 liblldb-14-dev

WORKDIR /app
COPY . .

RUN python3 scripts/build.py --build-type Release --install
```

## Getting Help

If you encounter issues during the build process:

1. Check the [GitHub Issues](https://github.com/your-repo/cangjie-debugger/issues)
2. Review build logs for specific error messages
3. Ensure all prerequisites are properly installed
4. Try building with minimal configuration first
5. Use the `--skip-deps-check` flag if dependency checking fails

For additional support, create an issue with:
- Operating system and version
- Build command used
- Full error output
- CMake and compiler versions