
# ============================================
# Native compilation toolchain file
# Windows AMD64 (x86-64)
# ============================================

# Target system (native build)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Platform identification
set(CANGJIE_TARGET_OS "windows")
set(CANGJIE_TARGET_ARCH "amd64")
set(CANGJIE_OUTPUT_SUFFIX "windows_amd64")

# For native Windows builds, use default compilers
# MSVC will be auto-detected on Windows with Visual Studio
# MinGW can be specified via CMAKE_C_COMPILER/CMAKE_CXX_COMPILER

# AMD64 specific compiler flags
if(MSVC)
    # MSVC specific flags for AMD64
    set(CMAKE_C_FLAGS_INIT "/DWIN32 /D_WINDOWS")
    set(CMAKE_CXX_FLAGS_INIT "/DWIN32 /D_WINDOWS")

    # Optimization flags
    set(CMAKE_C_FLAGS_RELEASE_INIT "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE_INIT "/O2 /DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG_INIT "/Od /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Od /Zi")
else()
    # GCC/MinGW specific flags
    set(CMAKE_C_FLAGS_INIT "-m64 -march=x86-64")
    set(CMAKE_CXX_FLAGS_INIT "-m64 -march=x86-64")

    # Static linking for standalone binary
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

    # Optimization flags
    set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")
    set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")
    set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")
endif()

# Windows-specific definitions
add_definitions(-DWIN32 -D_WIN32 -DWINDOWS)
add_definitions(-DUNICODE -D_UNICODE)

message(STATUS "====================================")
message(STATUS "Native Windows AMD64 toolchain:")
message(STATUS "  System: Windows")
message(STATUS "  Processor: AMD64 (x86-64)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
if(MSVC)
    message(STATUS "  Compiler: MSVC")
else()
    message(STATUS "  Compiler: GCC/MinGW")
endif()
message(STATUS "====================================")