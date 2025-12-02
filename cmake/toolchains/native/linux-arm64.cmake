# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# ============================================
# Native compilation toolchain file
# Linux ARM64 (aarch64)
# ============================================

# Target system (native build on ARM64 Linux)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Platform identification
set(CANGJIE_TARGET_OS "linux")
set(CANGJIE_TARGET_ARCH "arm64")
set(CANGJIE_OUTPUT_SUFFIX "linux_arm64")

# For native ARM64 Linux builds, use default GCC/Clang compilers
# Can be overridden via CMAKE_C_COMPILER/CMAKE_CXX_COMPILER

# ARM64 specific compiler flags
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags for different build types
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=armv8-a")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=armv8-a")

set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")

set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")

set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")

# Linux-specific definitions
add_definitions(-D_GNU_SOURCE)
add_definitions(-DLINUX)

message(STATUS "====================================")
message(STATUS "Native Linux ARM64 toolchain:")
message(STATUS "  System: Linux")
message(STATUS "  Processor: aarch64 (ARM64)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")