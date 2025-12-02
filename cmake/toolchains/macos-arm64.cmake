
# ============================================
# Native compilation toolchain file
# macOS ARM64 (Apple Silicon)
# ============================================

# Target system (native build on Apple Silicon Mac)
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# Platform identification
set(CANGJIE_TARGET_OS "macos")
set(CANGJIE_TARGET_ARCH "arm64")
set(CANGJIE_OUTPUT_SUFFIX "macos_arm64")

# macOS specific settings
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# For native macOS builds, use default Clang compiler
# Apple Clang is recommended for macOS

# ARM64 specific compiler flags for macOS
set(CMAKE_C_FLAGS_INIT "-arch arm64")
set(CMAKE_CXX_FLAGS_INIT "-arch arm64")

# Position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags for different build types
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")

set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")

# macOS-specific definitions
add_definitions(-DMACOS -D__APPLE__)

# Use libc++ on macOS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

message(STATUS "====================================")
message(STATUS "Native macOS ARM64 toolchain:")
message(STATUS "  System: macOS (Darwin)")
message(STATUS "  Processor: arm64 (Apple Silicon)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "  Min deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "====================================")