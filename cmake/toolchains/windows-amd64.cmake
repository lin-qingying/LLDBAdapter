# ============================================
# LLVM-MinGW toolchain file
# Windows AMD64 (x86-64)
# ============================================

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Platform identification
set(CANGJIE_TARGET_OS "windows")
set(CANGJIE_TARGET_ARCH "amd64")
set(CANGJIE_OUTPUT_SUFFIX "windows_amd64")

# -------------------------------
# LLVM-MinGW compilers
# -------------------------------
# These executables come from:
#   llvm-mingw/bin/clang.exe
#   llvm-mingw/bin/clang++.exe
#   llvm-mingw/bin/llvm-rc.exe
#   llvm-mingw/bin/llvm-ar.exe
#   llvm-mingw/bin/lld-link.exe
# They are inserted into PATH by GitHub Actions.
# -------------------------------

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_AR llvm-ar)
set(CMAKE_LINKER lld-link)

# Explicit MinGW target (very important)
set(CMAKE_C_COMPILER_TARGET x86_64-w64-mingw32)
set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-mingw32)

# Force MSVC-style linker mode for Windows
set(CMAKE_EXE_LINKER_FLAGS "/machine:x64")
set(CMAKE_SHARED_LINKER_FLAGS "/machine:x64")

# Optimization flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=x86-64 -mtune=generic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=x86-64 -mtune=generic")

set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Windows definitions
add_definitions(-DWIN32 -D_WIN32 -DWINDOWS)
add_definitions(-DUNICODE -D_UNICODE)

# Minimum supported Windows: Win7 (0x0601)
# LLVM-MinGW does NOT introduce APIs like clock_gettime64
add_definitions(-D_WIN32_WINNT=0x0601 -DWINVER=0x0601)

message(STATUS "====================================")
message(STATUS "LLVM-MinGW Windows AMD64 toolchain:")
message(STATUS "  System: Windows")
message(STATUS "  Processor: AMD64 (x86-64)")
message(STATUS "  Compiler: LLVM/Clang (llvm-mingw)")
message(STATUS "  Target: x86_64-w64-mingw32")
message(STATUS "  Linker: lld-link")
message(STATUS "  Resource compiler: llvm-rc")
message(STATUS "  Windows version: 7+ (0x0601)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")
