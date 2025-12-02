# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

cmake_minimum_required(VERSION 3.16.3)

project(CangjieDebugger VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")

# ============================================
# 静态链接 libstdc++ 和 libgcc（仅针对主程序）
# ============================================
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 静态链接标准C++库和GCC运行时库
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")

    message(STATUS "================================")
    message(STATUS "Static linking configuration:")
    message(STATUS "  - libstdc++ will be statically linked into the executable")
    message(STATUS "  - libgcc will be statically linked into the executable")
    message(STATUS "  - Third-party DLLs (like liblldb.dll) will still use their own dynamic dependencies")
    message(STATUS "================================")
endif()

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置输出目录为 'output'
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)

# 多配置生成器（如 Visual Studio）
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/output)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/output)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/output)
endforeach()

# 编译器特定选项
if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# 导出编译命令
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src/google)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 查找所需的包
find_package(PkgConfig)
if(NOT PkgConfig_FOUND)
    message(STATUS "未找到 pkg-config，继续执行但不使用它")
    set(PKG_CONFIG_EXECUTABLE "")
endif()

# 包含 cmake 模块
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 从本地 third_party 源自动构建 protobuf（带回退机制）
message(STATUS "设置自动 protobuf 构建...")
set(PROTOBUF_AUTO_BUILD_ENABLED ON CACHE BOOL "启用自动 protobuf 构建")

if(PROTOBUF_AUTO_BUILD_ENABLED)
    # Try to use existing BuildProtobuf.cmake first
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildProtobuf.cmake)
        message(STATUS "Found BuildProtobuf.cmake, using it to build protobuf...")
        include(BuildProtobuf)
        build_protobuf()

        # Verify protobuf was built successfully
        if(NOT PROTOBUF_LIBRARIES OR NOT PROTOBUF_INCLUDE_DIRS OR NOT PROTOBUF_PROTOC_EXECUTABLE)
            message(FATAL_ERROR "BuildProtobuf.cmake failed to build protobuf")
        endif()

        message(STATUS "Protobuf built successfully via BuildProtobuf.cmake")
        message(STATUS "Protobuf include dirs: ${PROTOBUF_INCLUDE_DIRS}")
        message(STATUS "Protobuf libraries: ${PROTOBUF_LIBRARIES}")
        message(STATUS "Protoc executable: ${PROTOBUF_PROTOC_EXECUTABLE}")
    else()
        # Fallback to built-in protobuf build logic
        message(STATUS "BuildProtobuf.cmake not found, using built-in protobuf build logic...")

        set(PROTOBUF_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-build)
        set(PROTOBUF_INSTALL_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-install)
        set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)

        # Check if protobuf is already built
        if(EXISTS ${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX} AND
                EXISTS ${PROTOBUF_INSTALL_DIR}/include/google/protobuf/message.h)
            message(STATUS "Found existing protobuf build at ${PROTOBUF_INSTALL_DIR}")
        else()
            # Build protobuf from source
            message(STATUS "Building protobuf from source at ${PROTOBUF_SOURCE_DIR}")

            if(NOT EXISTS ${PROTOBUF_SOURCE_DIR}/CMakeLists.txt)
                message(FATAL_ERROR "Protobuf source not found at ${PROTOBUF_SOURCE_DIR}")
            endif()

            file(MAKE_DIRECTORY ${PROTOBUF_BUILD_DIR})

            # Configure protobuf build
            execute_process(
                    COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR}
                    -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_DIR}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -Dprotobuf_BUILD_TESTS=OFF
                    -Dprotobuf_BUILD_EXAMPLES=OFF
                    -Dprotobuf_BUILD_PROTOC_BINARIES=ON
                    -Dprotobuf_BUILD_SHARED_LIBS=OFF
                    -Dprotobuf_WITH_ZLIB=OFF
                    -Dprotobuf_WITH_RTTI=ON
                    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                    -Dprotobuf_INSTALL=ON
                    ${PROTOBUF_SOURCE_DIR}
                    WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
                    RESULT_VARIABLE PROTOBUF_CONFIGURE_RESULT
            )

            if(NOT PROTOBUF_CONFIGURE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to configure protobuf")
            endif()

            # Build protobuf
            execute_process(
                    COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install --parallel
                    WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
                    RESULT_VARIABLE PROTOBUF_BUILD_RESULT
            )

            if(NOT PROTOBUF_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to build protobuf")
            endif()

            message(STATUS "Protobuf build completed successfully")
        endif()

        # Set protobuf variables - include both built and source headers
        set(PROTOBUF_INCLUDE_DIRS
                ${PROTOBUF_INSTALL_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src
        )
        set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX})

        # Find protobuf libraries automatically
        find_library(PROTOBUF_LIBRARY
                NAMES protobuf libprotobuf protobufd libprotobufd
                PATHS ${PROTOBUF_INSTALL_DIR}/lib
                NO_DEFAULT_PATH
        )

        find_library(PROTOC_LIBRARY
                NAMES protoc libprotoc protocd libprotocd
                PATHS ${PROTOBUF_INSTALL_DIR}/lib
                NO_DEFAULT_PATH
        )

        if(NOT PROTOBUF_LIBRARY OR NOT PROTOC_LIBRARY)
            message(FATAL_ERROR "Failed to find protobuf libraries in ${PROTOBUF_INSTALL_DIR}/lib")
        endif()

        # Find all absl libraries automatically
        file(GLOB ALL_ABSL_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libabsl_*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/absl_*.lib"
        )

        if(ALL_ABSL_LIBRARIES)
            list(LENGTH ALL_ABSL_LIBRARIES ABSL_COUNT)
            message(STATUS "Found ${ABSL_COUNT} absl libraries")
        else()
            message(STATUS "No absl libraries found in ${PROTOBUF_INSTALL_DIR}/lib")
        endif()

        # Find all utf8-related libraries (utf8_range, utf8_validity, etc.)
        file(GLOB UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libutf8*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/utf8*.lib"
        )

        if(UTF8_LIBRARIES)
            list(LENGTH UTF8_LIBRARIES UTF8_COUNT)
            message(STATUS "Found ${UTF8_COUNT} utf8 libraries: ${UTF8_LIBRARIES}")
        else()
            message(STATUS "No utf8 libraries found in ${PROTOBUF_INSTALL_DIR}/lib")
        endif()

        # Find all upb-related libraries
        file(GLOB UPB_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libupb*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/upb*.lib"
        )

        if(UPB_LIBRARIES)
            list(LENGTH UPB_LIBRARIES UPB_COUNT)
            message(STATUS "Found ${UPB_COUNT} upb libraries: ${UPB_LIBRARIES}")
        else()
            message(STATUS "No upb libraries found in ${PROTOBUF_INSTALL_DIR}/lib")
        endif()

        # Create a comprehensive list of all libraries
        set(ALL_LIBRARIES
                ${PROTOBUF_LIBRARY}
                ${PROTOC_LIBRARY}
                ${ALL_ABSL_LIBRARIES}
                ${UTF8_LIBRARIES}
                ${UPB_LIBRARIES}
        )

        # Set final protobuf libraries with ALL libraries found
        set(PROTOBUF_LIBRARIES ${ALL_LIBRARIES})

        # Debug output: show all libraries being linked
        message(STATUS "=== Protobuf Library Link Information ===")
        message(STATUS "Protobuf library: ${PROTOBUF_LIBRARY}")
        message(STATUS "Protoc library: ${PROTOC_LIBRARY}")
        message(STATUS "All absl libraries: ${ALL_ABSL_LIBRARIES}")
        message(STATUS "UTF8 Range library: ${UTF8_RANGE_LIBRARY}")
        message(STATUS "Total library count: ${CMAKE_LIST_LENGTH}")
        foreach(LIB ${ALL_LIBRARIES})
            message(STATUS "  -> ${LIB}")
        endforeach()
        message(STATUS "=============================================")

        message(STATUS "Protobuf built successfully via built-in logic")
        message(STATUS "Protobuf include dirs: ${PROTOBUF_INCLUDE_DIRS}")
        message(STATUS "Protobuf libraries: ${PROTOBUF_LIBRARIES}")
        message(STATUS "Protoc executable: ${PROTOBUF_PROTOC_EXECUTABLE}")
    endif()
else()
    # Use pre-built protobuf from third_party/protobuf/cmake-build-debug (fallback)
    message(STATUS "Using pre-built protobuf (auto-build disabled)...")
    set(PROTOBUF_PROTOC_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/cmake-build-debug/protoc${CMAKE_EXECUTABLE_SUFFIX}")
    set(PROTOBUF_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src")

    # Collect all absl static libraries from the pre-built protobuf
    file(GLOB ALL_ABSL_LIBRARIES
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/cmake-build-debug/_deps/absl-build/*/lib*.a"
    )

    # Also collect utf8_range and other third-party libraries
    file(GLOB OTHER_LIBRARIES
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/cmake-build-debug/_deps/*/lib*.a"
    )

    set(PROTOBUF_LIBRARIES
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/cmake-build-debug/libprotobufd.a"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/cmake-build-debug/libprotocd.a"
            ${ALL_ABSL_LIBRARIES}
            ${OTHER_LIBRARIES}
    )

    # Debug output for fallback path
    message(STATUS "=== Fallback Protobuf Library Information ===")
    message(STATUS "Abseil libraries found: ${ALL_ABSL_LIBRARIES}")
    message(STATUS "Other libraries found: ${OTHER_LIBRARIES}")
    message(STATUS "Total libraries for fallback: ${CMAKE_LIST_LENGTH}")
    foreach(LIB ${PROTOBUF_LIBRARIES})
        message(STATUS "  -> ${LIB}")
    endforeach()
    message(STATUS "============================================")
endif()

# Verify protoc is available
if(NOT EXISTS ${PROTOBUF_PROTOC_EXECUTABLE})
    message(FATAL_ERROR "Protoc executable not found at: ${PROTOBUF_PROTOC_EXECUTABLE}")
endif()

message(STATUS "Using protoc: ${PROTOBUF_PROTOC_EXECUTABLE}")
message(STATUS "Protobuf include dirs: ${PROTOBUF_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${PROTOBUF_LIBRARIES}")

# Find Threads
find_package(Threads REQUIRED)

# Add protobuf include directories
include_directories(${PROTOBUF_INCLUDE_DIRS})

# LLVM and LLDB configuration
# Try to download LLVM if not present
include(DownloadLLVM)
download_llvm()

# Add include directories for LLVM/LLDB headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/llvm-project/llvm/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/llvm-project/lldb/include)

# Protobuf compilation
set(PROTO_FILES
        schema/event.proto
        schema/model.proto
        schema/request.proto
        schema/response.proto
)

# Create generated directory
set(GENERATED_PROTO_DIR ${CMAKE_BINARY_DIR}/generated/proto)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

# Auto-generate protobuf files with dependency checking
message(STATUS "Setting up automatic protobuf generation...")

# Function to check if proto file is newer than generated files
function(is_proto_newer PROTO_FILE HEADER_FILE SOURCE_FILE RESULT_VAR)
    set(${RESULT_VAR} FALSE PARENT_SCOPE)

    if(NOT EXISTS ${HEADER_FILE} OR NOT EXISTS ${SOURCE_FILE})
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Get modification times
    file(TIMESTAMP ${PROTO_FILE} PROTO_TIME "%s")
    file(TIMESTAMP ${HEADER_FILE} HEADER_TIME "%s")
    file(TIMESTAMP ${SOURCE_FILE} SOURCE_TIME "%s")

    # Check if proto file is newer than either generated file
    if(PROTO_TIME GREATER HEADER_TIME OR PROTO_TIME GREATER SOURCE_TIME)
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
    endif()
endfunction()

# Generate protobuf files with automatic re-generation
set(NEED_REGENERATION FALSE)

# Check all proto files for updates
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_HEADER ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.h)
    set(PROTO_SOURCE ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.cc)

    is_proto_newer(${CMAKE_SOURCE_DIR}/${PROTO_FILE} ${PROTO_HEADER} ${PROTO_SOURCE} SHOULD_REBUILD)

    if(SHOULD_REBUILD)
        set(NEED_REGENERATION TRUE)
        break()
    endif()
endforeach()

if(NEED_REGENERATION)
    message(STATUS "Detected changes in proto files, regenerating...")

    # Clear generated directory to ensure clean rebuild
    file(REMOVE_RECURSE ${GENERATED_PROTO_DIR})
    file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

    # Generate all protobuf files
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        message(STATUS "Generating ${PROTO_NAME}.pb.h and ${PROTO_NAME}.pb.cc...")

        # Execute protoc at configure time
        execute_process(
                COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
                --proto_path=${CMAKE_SOURCE_DIR}/schema
                --cpp_out=${GENERATED_PROTO_DIR}
                ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE PROTOC_RESULT
                ERROR_VARIABLE PROTOC_ERROR
                OUTPUT_VARIABLE PROTOC_OUTPUT
                ERROR_STRIP_TRAILING_WHITESPACE
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT PROTOC_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to generate protobuf for ${PROTO_FILE}: ${PROTOC_ERROR}")
        endif()
    endforeach()

    message(STATUS "All protobuf files regenerated successfully in: ${GENERATED_PROTO_DIR}")
else()
    message(STATUS "Proto files are up to date, skipping generation")
endif()

# Collect all generated protobuf files
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_SRCS ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.cc)
    set(PROTO_HDRS ${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.h)

    # Verify generated files exist
    if(NOT EXISTS ${PROTO_SRCS} OR NOT EXISTS ${PROTO_HDRS})
        message(FATAL_ERROR "Generated protobuf files not found for ${PROTO_FILE}")
    endif()

    list(APPEND ALL_PROTO_SRCS ${PROTO_SRCS})
    list(APPEND ALL_PROTO_HDRS ${PROTO_HDRS})
endforeach()

message(STATUS "Collected protobuf source files: ${ALL_PROTO_SRCS}")
message(STATUS "Collected protobuf header files: ${ALL_PROTO_HDRS}")

# Add build-time regeneration target for manual regeneration
add_custom_target(regenerate_protos
        COMMAND ${CMAKE_COMMAND} -E echo "Force regenerating all protobuf files..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${GENERATED_PROTO_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_PROTO_DIR}
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
        --proto_path=${CMAKE_SOURCE_DIR}/schema
        --cpp_out=${GENERATED_PROTO_DIR}
        ${PROTO_FILES}
        COMMENT "Force regenerating all protobuf files"
        VERBATIM
)

# Add custom target to rebuild protobuf from source
add_custom_target(rebuild_protobuf
        COMMAND ${CMAKE_COMMAND} -E echo "Rebuilding protobuf from source..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/third_party/protobuf-build
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/third_party/protobuf-install
        COMMENT "Rebuilding protobuf from source (will trigger CMake reconfigure)"
)

# Make regenerate_protos depend on rebuild_protobuf for complete rebuild
add_dependencies(regenerate_protos rebuild_protobuf)

# Add the generated directory to include paths
include_directories(${GENERATED_PROTO_DIR})

# Source files
set(CORE_SOURCES
        src/core/BreakpointManager.cpp

)

set(PROTOCOL_SOURCES
        src/protocol/ProtoConverter.cpp
)

set(UTILS_SOURCES
        src/utils/Logger.cpp
)

set(CLIENT_SOURCES
        src/client/TcpClient.cpp
        src/client/DebuggerClient.cpp
        src/client/DebuggerClientHandlers.cpp
        src/client/DebuggerClientResponse.cpp
        src/client/DebuggerClientUtils.cpp
        src/client/TcpClient.cpp
        src/client/DebuggerClientEvents.cpp
)

# Collect all sources (now including protobuf sources)
set(ALL_SOURCES
        ${CORE_SOURCES}
        ${PROTOCOL_SOURCES}
        ${UTILS_SOURCES}
        ${CLIENT_SOURCES}
        ${ALL_PROTO_SRCS}

)

# ============================================
# Determine output suffix based on platform
# ============================================
# Check if toolchain file set these variables
if(NOT DEFINED CANGJIE_OUTPUT_SUFFIX)
    # Auto-detect platform if not set by toolchain file
    if(WIN32)
        set(CANGJIE_TARGET_OS "windows")
    elseif(APPLE)
        set(CANGJIE_TARGET_OS "macos")
    else()
        set(CANGJIE_TARGET_OS "linux")
    endif()

    # Detect architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(CANGJIE_TARGET_ARCH "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
        set(CANGJIE_TARGET_ARCH "arm32")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        set(CANGJIE_TARGET_ARCH "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386|x86")
        set(CANGJIE_TARGET_ARCH "x86")
    else()
        set(CANGJIE_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(CANGJIE_OUTPUT_SUFFIX "${CANGJIE_TARGET_OS}_${CANGJIE_TARGET_ARCH}")
endif()

message(STATUS "====================================")
message(STATUS "Target Platform Configuration:")
message(STATUS "  OS: ${CANGJIE_TARGET_OS}")
message(STATUS "  Architecture: ${CANGJIE_TARGET_ARCH}")
message(STATUS "  Output suffix: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")

# Create single executable: CangJieLLDBAdapter_<os>_<arch>
set(EXECUTABLE_NAME "CangJieLLDBAdapter_${CANGJIE_OUTPUT_SUFFIX}")
add_executable(${EXECUTABLE_NAME}
        src/main.cpp
        ${ALL_SOURCES}
)

# Set C++17 standard
set_property(TARGET ${EXECUTABLE_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${EXECUTABLE_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Note: Do NOT define PROTOBUF_USE_DLLS - let protobuf use its default settings
# The generated code must match the library's compilation settings

# Suppress warnings in protobuf generated files and includes
foreach(PROTO_SRC ${ALL_PROTO_SRCS})
    # Add warning suppression for protobuf generated files
    set_source_files_properties(${PROTO_SRC} PROPERTIES
            COMPILE_OPTIONS "-Wno-sign-compare -Wno-unused-parameter"
    )
endforeach()

# Also suppress warnings for protobuf headers when compiling other files
target_compile_options(${EXECUTABLE_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-sign-compare>
)

# Link libraries with proper grouping for static libraries
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Use --whole-archive to force linking all symbols from static libraries
    # This prevents Release mode optimization from removing symbols like ThreadSafeArena::thread_cache()
    target_link_libraries(${EXECUTABLE_NAME}
            -Wl,--whole-archive
            ${PROTOBUF_LIBRARIES}
            -Wl,--no-whole-archive
            Threads::Threads
    )
else()
    target_link_libraries(${EXECUTABLE_NAME}
            ${PROTOBUF_LIBRARIES}
            Threads::Threads
    )
endif()

# Add Windows-specific libraries if needed
if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME}
            kernel32     # For ConPTY functions (CreatePseudoConsole, etc.)
            dbghelp      # For symbolization functions like SymInitialize, SymFromAddr
            psapi        # For process API functions
            wsock32      # For socket functions
            ws2_32       # For Windows Sockets 2
            bcrypt       # For BCrypt cryptographic functions (used by absl random)
    )
endif()

# LLDB 动态链接配置 - 根据平台和架构使用不同的库文件
if(WIN32)
    # Windows 平台：使用 liblldb_windows_<arch>.dll
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.dll")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")

    message(STATUS "Windows LLDB library: ${LLDB_LIB_PATH}")

    target_link_libraries(${EXECUTABLE_NAME}
            ${LLDB_LIB_PATH}
            psapi
            ws2_32
            wsock32
    )
elseif(APPLE)
    # macOS 平台：使用 liblldb_macos_<arch>.dylib
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.dylib")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")

    message(STATUS "macOS LLDB library: ${LLDB_LIB_PATH}")

    target_link_libraries(${EXECUTABLE_NAME}
            ${LLDB_LIB_PATH}
    )
else()
    # Linux 平台：使用 liblldb_linux_<arch>.so
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.so")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")

    message(STATUS "Linux LLDB library: ${LLDB_LIB_PATH}")

    target_link_libraries(${EXECUTABLE_NAME}
            ${LLDB_LIB_PATH}
    )
endif()

# Installation
install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(FILES ${ALL_PROTO_HDRS} DESTINATION include/cangjie/debugger)
install(DIRECTORY include/cangjie/debugger DESTINATION include)

# Testing
enable_testing()


# Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CangjieDebuggerConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CangjieDebuggerConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CangjieDebuggerConfig.cmake"
        INSTALL_DESTINATION lib/cmake/CangjieDebugger
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CangjieDebuggerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CangjieDebuggerConfigVersion.cmake"
        DESTINATION lib/cmake/CangjieDebugger
)

# ============================================
# 复制所需的 DLL 到输出目录（仅 Debug 模式）
# ============================================
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # 分类说明DLL的用途
    message(STATUS "================================")
    message(STATUS "DLL Dependency Configuration (Debug Mode Only):")
    message(STATUS "  Main executable: Statically links libstdc++ and libgcc")
    message(STATUS "  Third-party DLLs: Keep their own dynamic dependencies")
    message(STATUS "================================")

    # LLVM/LLDB 相关 DLLs（必需）
    set(LLVM_DLLS
            libLLVM-15.dll
            libLTO.dll
            libclang-cpp.dll
            libclang.dll
            liblldb.dll
    )

    # Cangjie 语言支持 DLLs（必需）
    set(CANGJIE_DLLS
            libcangjie-frontend.dll
            libcangjie-lsp.dll
    )

    # 第三方依赖 DLLs（必需）
    set(THIRD_PARTY_DLLS
            libxml2.dll
    )

    # 运行时库 DLLs（liblldb.dll等第三方DLL所需）
    # 注意：主程序不需要这些，但第三方DLL需要
    set(RUNTIME_DLLS
            libstdc++-6.dll      # liblldb_windows_amd64.dll 和其他第三方DLL依赖
            libgcc_s_seh-1.dll   # liblldb_windows_amd64.dll 和其他第三方DLL依赖
            libwinpthread-1.dll  # liblldb_windows_amd64.dll 和其他第三方DLL依赖
            libssp-0.dll         # 可能被某些第三方DLL依赖
    )

    # 合并所有需要的DLLs
    set(REQUIRED_DLLS
            ${LLVM_DLLS}
            ${CANGJIE_DLLS}
            ${THIRD_PARTY_DLLS}
            ${RUNTIME_DLLS}
    )

    # 复制每个DLL到输出目录（仅在 Debug 模式）
    foreach(DLL_FILE ${REQUIRED_DLLS})
        set(DLL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/${DLL_FILE}")
        if(EXISTS ${DLL_SOURCE})
            add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL_SOURCE}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DLL_FILE}
                    COMMENT "Copying ${DLL_FILE} to output directory (Debug mode)"
            )
        else()
            message(WARNING "DLL not found: ${DLL_SOURCE}")
        endif()
    endforeach()

    message(STATUS "================================")
    message(STATUS "DLL Copy Configuration:")
    message(STATUS "  Build Type: Debug (DLLs will be copied)")
    message(STATUS "  Total DLLs to copy: ${REQUIRED_DLLS}")
    message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    message(STATUS "================================")
elseif(WIN32)
    message(STATUS "================================")
    message(STATUS "DLL Copy Configuration:")
    message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE} (DLLs will NOT be copied)")
    message(STATUS "  Note: DLLs are only copied in Debug mode")
    message(STATUS "================================")
endif()