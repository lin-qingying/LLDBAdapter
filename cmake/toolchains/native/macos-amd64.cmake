# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# ============================================
# Native compilation toolchain file
# macOS AMD64 (x86-64 Intel)
# ============================================

# Target system (native build on Intel Mac)
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Platform identification
set(CANGJIE_TARGET_OS "macos")
set(CANGJIE_TARGET_ARCH "amd64")
set(CANGJIE_OUTPUT_SUFFIX "macos_amd64")

# macOS specific settings
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Target architecture")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

# For native macOS builds, use default Clang compiler
# Apple Clang is recommended for macOS

# AMD64 specific compiler flags for macOS
set(CMAKE_C_FLAGS_INIT "-arch x86_64")
set(CMAKE_CXX_FLAGS_INIT "-arch x86_64")

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
message(STATUS "Native macOS AMD64 toolchain:")
message(STATUS "  System: macOS (Darwin)")
message(STATUS "  Processor: x86_64 (Intel)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "  Min deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "====================================")