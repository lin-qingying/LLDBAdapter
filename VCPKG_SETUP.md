# Cangjie Debugger - Vcpkg Protobuf Setup

This document explains how to set up and build the Cangjie Debugger project using vcpkg for Protobuf dependency management.

## Prerequisites

1. **Git** - Required for vcpkg installation
2. **CMake 3.16.5+** - Build system
3. **Visual Studio 2022** - For MSVC toolchain (optional)
4. **MinGW-w64** - For GCC toolchain (optional, can be installed via CLion bundle)

## Setup Instructions

### 1. Install vcpkg

If you haven't already installed vcpkg:

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git

# Bootstrap vcpkg
cd vcpkg
./bootstrap-vcpkg.bat  # On Windows
```

### 2. Install Protobuf

#### Using MSVC (Recommended for Windows):
```bash
cd vcpkg
./vcpkg.exe install protobuf:x64-windows
```

#### Using MinGW:
```bash
cd vcpkg
./vcpkg.exe install protobuf:x64-mingw-static
```

### 3. Configure Environment Variables

Set the VCPKG_ROOT environment variable to point to your vcpkg installation:

**Windows (Command Prompt):**
```cmd
set VCPKG_ROOT=D:\path\to\vcpkg
```

**Windows (PowerShell):**
```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
```

### 4. Build the Project

#### Using CMake Presets (Recommended):

```bash
# List available presets
cmake --list-presets

# Configure with vcpkg (MSVC)
cmake --preset windows-vcpkg

# Configure with MinGW
cmake --preset windows-mingw

# Build
cmake --build --preset windows-release
```

#### Using Traditional CMake:

**With MSVC:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

**With MinGW:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build . --config Release
```

## Toolchain Configuration

### MSVC Toolchain
- Uses Visual Studio 2022 compiler
- vcpkg triplet: `x64-windows`
- Recommended for Windows development

### MinGW Toolchain
- Uses GCC compiler
- vcpkg triplet: `x64-mingw-static`
- Static linking for better portability
- Use CLion's bundled MinGW or install separately

## Project Structure

```
cangjie_debugger/
├── CMakeLists.txt              # Main CMake configuration
├── CMakePresets.json           # CMake presets for easy configuration
├── cmake/
│   └── mingw-toolchain.cmake   # MinGW toolchain helper
├── schema/                     # Protobuf schema files
├── src/                        # Source code
└── VCPKG_SETUP.md              # This file
```

## Troubleshooting

### Protobuf not found
1. Ensure vcpkg is properly installed
2. Check that VCPKG_ROOT environment variable is set
3. Verify the correct triplet is being used
4. Try cleaning the build directory and reconfiguring

### MinGW compiler not found
1. Install MinGW-w64 or use CLion's bundled version
2. Ensure MinGW bin directory is in PATH
3. Check the toolchain file for proper compiler detection

### Build failures
1. Check CMake version (minimum 3.16.5)
2. Verify all dependencies are installed
3. Try using the provided CMake presets
4. Check build logs for specific error messages

## Notes

- The project is configured to work with both MSVC and MinGW toolchains
- vcpkg automatically handles dependency management
- CMake presets provide an easy way to switch between configurations
- The MinGW toolchain file helps locate compilers without hardcoding paths