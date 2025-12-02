# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

# ============================================
# Native compilation toolchain file
# Linux AMD64 (x86-64)
# ============================================

# Target system (native build)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Platform identification
set(CANGJIE_TARGET_OS "linux")
set(CANGJIE_TARGET_ARCH "amd64")
set(CANGJIE_OUTPUT_SUFFIX "linux_amd64")

# For native Linux builds, use default GCC/Clang compilers
# Can be overridden via CMAKE_C_COMPILER/CMAKE_CXX_COMPILER

# AMD64 specific compiler flags
set(CMAKE_C_FLAGS_INIT "-m64 -march=x86-64")
set(CMAKE_CXX_FLAGS_INIT "-m64 -march=x86-64")

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags for different build types
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -march=x86-64 -mtune=generic")

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
message(STATUS "Native Linux AMD64 toolchain:")
message(STATUS "  System: Linux")
message(STATUS "  Processor: x86_64 (AMD64)")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")