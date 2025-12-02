
cmake_minimum_required(VERSION 3.16.3)

project(CangjieDebugger VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================
# 静态链接标准库（所有平台）
# ============================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC/MinGW: 静态链接标准C++库和GCC运行时库
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")

    message(STATUS "================================")
    message(STATUS "GCC 静态链接配置:")
    message(STATUS "  - libstdc++ 将被静态链接")
    message(STATUS "  - libgcc 将被静态链接")
    message(STATUS "  - protobuf 库静态链接 (.a)")
    if(WIN32)
        message(STATUS "  - 第三方 DLLs (liblldb.dll) 使用它们自己的依赖")
    else()
        message(STATUS "  - 第三方 .so 文件 (liblldb.so) 使用它们自己的依赖")
    endif()
    message(STATUS "================================")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang: 静态链接标准C++库
    if(APPLE)
        # macOS 默认使用 libc++
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
        message(STATUS "================================")
        message(STATUS "Clang 静态链接配置 (macOS):")
        message(STATUS "  - libc++ 将被静态链接")
        message(STATUS "  - protobuf 库静态链接 (.a)")
        message(STATUS "================================")
    else()
        # Linux Clang 使用 libstdc++
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
        message(STATUS "================================")
        message(STATUS "Clang 静态链接配置 (Linux):")
        message(STATUS "  - libstdc++ 将被静态链接")
        message(STATUS "  - protobuf 库静态链接 (.a)")
        message(STATUS "================================")
    endif()
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

# 编译器选项（GCC/Clang）
add_compile_options(-Wall -Wextra -Werror)

# 导出编译命令
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src/google)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)



# 包含 cmake 模块
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================
# Protobuf 配置（支持交叉编译）
# ============================================
# 检查是否为交叉编译模式
if(CMAKE_CROSSCOMPILING AND DEFINED PROTOC_HOST)
    # 交叉编译模式：使用宿主机 protoc 和目标平台 protobuf 库
    message(STATUS "====================================")
    message(STATUS "交叉编译模式")
    message(STATUS "  使用宿主机 protoc: ${PROTOC_HOST}")
    message(STATUS "  构建目标平台 protobuf 库")
    message(STATUS "====================================")

    include(BuildProtobufTarget)
    build_protobuf_target(${PROTOC_HOST})
else()
    # 本地编译模式：构建 protobuf 和 protoc
    message(STATUS "====================================")
    message(STATUS "本地编译模式")
    message(STATUS "  构建 protobuf 和 protoc")
    message(STATUS "====================================")

    include(BuildProtobuf)
    build_protobuf()
endif()

# 查找线程库
find_package(Threads REQUIRED)

# LLVM 和 LLDB 配置
# 如果不存在则尝试下载 LLVM
include(DownloadLLVM)
download_llvm()

# 添加 LLVM/LLDB 头文件的包目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/llvm-project/llvm/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/llvm-project/lldb/include)


# 源文件
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

# 收集所有源文件（现在包括 protobuf 源文件）
set(ALL_SOURCES
        ${CORE_SOURCES}
        ${PROTOCOL_SOURCES}
        ${UTILS_SOURCES}
        ${CLIENT_SOURCES}
        ${ALL_PROTO_SRCS}

)

# ============================================
# 根据平台确定输出后缀
# ============================================
# 检查工具链文件是否设置了这些变量
if(NOT DEFINED CANGJIE_OUTPUT_SUFFIX)
    # 如果工具链文件未设置，则自动检测平台
    if(WIN32)
        set(CANGJIE_TARGET_OS "windows")
    elseif(APPLE)
        set(CANGJIE_TARGET_OS "macos")
    else()
        set(CANGJIE_TARGET_OS "linux")
    endif()

    # 检测架构
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
message(STATUS "目标平台配置:")
message(STATUS "  操作系统: ${CANGJIE_TARGET_OS}")
message(STATUS "  架构: ${CANGJIE_TARGET_ARCH}")
message(STATUS "  输出后缀: ${CANGJIE_OUTPUT_SUFFIX}")
message(STATUS "====================================")

# 创建单个可执行文件: CangJieLLDBAdapter_<os>_<arch>
set(EXECUTABLE_NAME "CangJieLLDBAdapter_${CANGJIE_OUTPUT_SUFFIX}")
add_executable(${EXECUTABLE_NAME}
        src/main.cpp
        ${ALL_SOURCES}
)

# 设置 C++17 标准
set_property(TARGET ${EXECUTABLE_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${EXECUTABLE_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# 注意：不要定义 PROTOBUF_USE_DLLS - 让 protobuf 使用其默认设置
# 生成的代码必须与库的编译设置匹配

# 抑制 protobuf 生成文件和包含文件中的警告
foreach(PROTO_SRC ${ALL_PROTO_SRCS})
    # 为 protobuf 生成文件添加警告抑制
    set_source_files_properties(${PROTO_SRC} PROPERTIES
            COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-parameter"
    )
endforeach()

# 同时在编译其他文件时抑制 protobuf 头文件的警告
target_compile_options(${EXECUTABLE_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-sign-compare>
)


# 链接 protobuf 和其他库（使用 CMake targets）
# protobuf::libprotobuf target 自动包含所有依赖（absl、utf8_range、upb 等）
target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
    protobuf::libprotobuf  # 自动处理所有 protobuf 依赖
    Threads::Threads
)

# 在 Linux/Unix 上添加 Abseil 所需的系统库
# 这些是依赖于 Abseil 的现代 Protobuf (3.21+/4.x) 所需的
if(UNIX AND NOT APPLE)
    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
        atomic  # absl::base 原子操作所需
        rt      # absl::synchronization POSIX 实时所需
    )
    message(STATUS "已添加 Linux 系统库: atomic, rt (Abseil 所需)")
endif()

# 如果需要，添加 Windows 特定的库
if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
            kernel32     # ConPTY 函数所需 (CreatePseudoConsole 等)
            dbghelp      # 符号化函数所需 (SymInitialize, SymFromAddr)
            psapi        # 进程 API 函数所需
            wsock32      # socket 函数所需
            ws2_32       # Windows Sockets 2 所需
            bcrypt       # BCrypt 加密函数所需 (absl random 使用)
    )
endif()

# LLDB 动态链接配置 - 根据平台和架构���用不同的库文件
if(WIN32)
    # Windows 平台：使用 liblldb_windows_<arch>.dll
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.dll")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")

    message(STATUS "Windows LLDB 库: ${LLDB_LIB_PATH}")

    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
            ${LLDB_LIB_PATH}
            psapi
            ws2_32
            wsock32
    )
elseif(APPLE)
    # macOS 平台：使用 liblldb_macos_<arch>.dylib
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.dylib")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")

    message(STATUS "macOS LLDB 库: ${LLDB_LIB_PATH}")

    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
            ${LLDB_LIB_PATH}
    )
else()
    # Linux 平台：使用 liblldb_linux_<arch>.so
    set(LLDB_LIB_NAME "liblldb_${CANGJIE_OUTPUT_SUFFIX}.so")
    set(LLDB_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/${LLDB_LIB_NAME}")
    set(LLDB_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib")

    message(STATUS "Linux LLDB 库: ${LLDB_LIB_PATH}")
    message(STATUS "Linux LLDB 库目录: ${LLDB_LIB_DIR}")

    # 设置 RPATH，以便运行时能找到依赖的共享库
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH "${LLDB_LIB_DIR}:\$ORIGIN/../lib:\$ORIGIN"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )

    # 设置链接时的库搜索路径
    link_directories(${LLDB_LIB_DIR})

    # 添加链接器选项：允许共享库有未定义的符号（运行时解析）
    target_link_options(${EXECUTABLE_NAME} PRIVATE
        "-Wl,--allow-shlib-undefined"
    )

    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
            ${LLDB_LIB_PATH}
            dl  # 动态链接库加载
    )
endif()

# 安装
install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(FILES ${ALL_PROTO_HDRS} DESTINATION include/cangjie/debugger)
install(DIRECTORY include/cangjie/debugger DESTINATION include)

# 测试
enable_testing()


# 文档
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "使用 Doxygen 生成 API 文档" VERBATIM
    )
endif()

# 包配置
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
    message(STATUS "DLL 依赖配置（仅限 Debug 模式）:")
    message(STATUS "  主可执行文件: 静态链接 libstdc++ 和 libgcc")
    message(STATUS "  第三方 DLLs: 保留它们自己的动态依赖")
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
                    COMMENT "复制 ${DLL_FILE} 到输出目录（Debug 模式）"
            )
        else()
            message(WARNING "未找到 DLL: ${DLL_SOURCE}")
        endif()
    endforeach()

    message(STATUS "================================")
    message(STATUS "DLL 复制配置:")
    message(STATUS "  构建类型: Debug (将复制 DLLs)")
    message(STATUS "  需要复制的 DLLs 总数: ${REQUIRED_DLLS}")
    message(STATUS "  输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    message(STATUS "================================")
elseif(WIN32)
    message(STATUS "================================")
    message(STATUS "DLL 复制配置:")
    message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE} (不会复制 DLLs)")
    message(STATUS "  注意: 仅在 Debug 模式下复制 DLLs")
    message(STATUS "================================")
endif()