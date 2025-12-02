# ============================================
# Protobuf 构建模块
# ============================================
# 此模块负责从源代码构建 protobuf 并使用 find_package 提供 CMake targets
# 同时处理 .proto 文件的生成
# 优势：自动处理所有传递依赖（absl、utf8_range、upb 等）

cmake_minimum_required(VERSION 3.16)

function(build_protobuf)
    message(STATUS "设置 Protobuf 配置...")

    # 设置 protobuf 构建和安装路径
    set(PROTOBUF_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-build)
    set(PROTOBUF_INSTALL_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-install)
    set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)
    set(PROTOBUF_CONFIG_FILE ${PROTOBUF_INSTALL_DIR}/lib/cmake/protobuf/protobuf-config.cmake)

    # 检查是否需要构建 protobuf
    if(NOT EXISTS ${PROTOBUF_CONFIG_FILE})
        message(STATUS "Protobuf 未找到，开始从源代码构建...")

        # 验证源代码存在
        if(NOT EXISTS ${PROTOBUF_SOURCE_DIR}/CMakeLists.txt)
            message(FATAL_ERROR "在 ${PROTOBUF_SOURCE_DIR} 未找到 Protobuf 源代码")
        endif()

        # 初始化子模块（如果有）
        if(EXISTS ${PROTOBUF_SOURCE_DIR}/.gitmodules)
            find_program(GIT_EXECUTABLE git)
            if(GIT_EXECUTABLE)
                message(STATUS "初始化 Protobuf 子模块...")
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${PROTOBUF_SOURCE_DIR}
                    OUTPUT_QUIET ERROR_QUIET
                )
            endif()
        endif()

        # 创建构建目录
        file(MAKE_DIRECTORY ${PROTOBUF_BUILD_DIR})

        # 配置 protobuf 构建
        message(STATUS "配置 Protobuf 构建...")
        message(STATUS "  这可能需要一些时间，请耐心等待...")

        # 为 Windows 设置额外的编译标志以确保兼容性
        set(EXTRA_CMAKE_FLAGS "")
        if(WIN32)
            list(APPEND EXTRA_CMAKE_FLAGS
                "-DCMAKE_C_FLAGS=-D_WIN32_WINNT=0x0601"
                "-DCMAKE_CXX_FLAGS=-D_WIN32_WINNT=0x0601"
            )
            message(STATUS "  添加 Windows 兼容性标志: _WIN32_WINNT=0x0601 (Windows 7+)")

            # 如果使用 Clang，确保 protobuf 也使用 libc++
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                list(APPEND EXTRA_CMAKE_FLAGS
                    "-DCMAKE_CXX_FLAGS=-D_WIN32_WINNT=0x0601 -stdlib=libc++"
                )
                message(STATUS "  使用 libc++ 标准库以匹配主程序")
            endif()
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR}
                -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_DIR}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_CXX_STANDARD=17
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -Dprotobuf_BUILD_TESTS=OFF
                -Dprotobuf_BUILD_EXAMPLES=OFF
                -Dprotobuf_BUILD_PROTOC_BINARIES=ON
                -Dprotobuf_BUILD_SHARED_LIBS=OFF
                -Dprotobuf_WITH_ZLIB=OFF
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -Dprotobuf_INSTALL=ON
                ${EXTRA_CMAKE_FLAGS}
                ${PROTOBUF_SOURCE_DIR}
            WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_CONFIGURE_RESULT
            # 移除 OUTPUT_VARIABLE 和 ERROR_VARIABLE 以显示实时输出
        )

        if(NOT PROTOBUF_CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "配置 Protobuf 失败")
        endif()

        # 构建 protobuf
        message(STATUS "构建 Protobuf...")
        message(STATUS "  编译中，进度会实时显示...")

        # 检测编译器类型，如果是 Clang 则限制并行数以避免链接器卡住
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(PARALLEL_JOBS 4)
            message(STATUS "  检测到 Clang 编译器：限制并行编译数为 ${PARALLEL_JOBS} 以避免链接器卡住")
        else()
            set(PARALLEL_JOBS "")
            message(STATUS "  使用默认并行编译数")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install --parallel ${PARALLEL_JOBS}
            WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_BUILD_RESULT
            # 移除 OUTPUT_VARIABLE 和 ERROR_VARIABLE 以显示实时输出
        )

        if(NOT PROTOBUF_BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "构建 Protobuf 失败")
        endif()

        message(STATUS "Protobuf 构建并安装成功到: ${PROTOBUF_INSTALL_DIR}")
    else()
        message(STATUS "找到已构建的 Protobuf: ${PROTOBUF_CONFIG_FILE}")
    endif()

    # 使用 find_package 查找已安装的 protobuf
    # 设置 CMAKE_PREFIX_PATH 让 CMake 能找到我们安装的 protobuf
    set(CMAKE_PREFIX_PATH ${PROTOBUF_INSTALL_DIR} ${CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)

    # 查找 protobuf 包（注意是小写 protobuf）
    find_package(protobuf CONFIG REQUIRED)

    # 显示 protobuf 配置信息
    message(STATUS "=================================")
    message(STATUS "Protobuf 配置信息:")
    message(STATUS "  版本: ${protobuf_VERSION}")
    message(STATUS "  包含目录: ${Protobuf_INCLUDE_DIRS}")
    message(STATUS "  protoc: $<TARGET_FILE:protobuf::protoc>")
    message(STATUS "  可用 targets:")
    message(STATUS "    - protobuf::libprotobuf")
    message(STATUS "    - protobuf::libprotoc")
    message(STATUS "    - protobuf::protoc")
    message(STATUS "=================================")

    # 添加 protobuf 包含目录（可选，因为 target 会自动包含）
    include_directories(${Protobuf_INCLUDE_DIRS})

    # ============================================
    # 生成 Proto 文件（在配置时立即生成）
    # ============================================
    message(STATUS "设置 protobuf 文件生成...")

    # Proto 文件列表（相对于 schema 目录的文件名）
    set(PROTO_FILE_NAMES
        event.proto
        model.proto
        request.proto
        response.proto
    )

    # 完整的 Proto 文件路径（用于依赖检查）
    set(PROTO_FILES)
    foreach(PROTO_NAME ${PROTO_FILE_NAMES})
        list(APPEND PROTO_FILES ${CMAKE_SOURCE_DIR}/schema/${PROTO_NAME})
    endforeach()

    # 创建生成目录
    set(GENERATED_PROTO_DIR ${CMAKE_BINARY_DIR}/generated/proto)
    file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

    # 生成 protobuf 文件列表
    set(ALL_PROTO_SRCS)
    set(ALL_PROTO_HDRS)

    foreach(PROTO_NAME ${PROTO_FILE_NAMES})
        get_filename_component(PROTO_BASE ${PROTO_NAME} NAME_WE)
        list(APPEND ALL_PROTO_SRCS ${GENERATED_PROTO_DIR}/${PROTO_BASE}.pb.cc)
        list(APPEND ALL_PROTO_HDRS ${GENERATED_PROTO_DIR}/${PROTO_BASE}.pb.h)
    endforeach()

    # 检查是否需要重新生成 proto 文件
    set(NEED_REGENERATE FALSE)

    # 检查生成的文件是否存在
    foreach(PROTO_SRC ${ALL_PROTO_SRCS})
        if(NOT EXISTS ${PROTO_SRC})
            set(NEED_REGENERATE TRUE)
            message(STATUS "Proto 文件不存在，需要生成: ${PROTO_SRC}")
            break()
        endif()
    endforeach()

    # 如果不存在或源文件更新，则在配置时立即生成
    if(NEED_REGENERATE)
        message(STATUS "在配置时生成 protobuf 文件...")

        # 获取 protoc 可执行文件路径
        get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION_RELEASE)
        if(NOT PROTOC_EXECUTABLE)
            get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION_DEBUG)
        endif()
        if(NOT PROTOC_EXECUTABLE)
            get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
        endif()

        # 如果仍然找不到，尝试从安装目录查找
        if(NOT PROTOC_EXECUTABLE)
            set(PROTOC_EXECUTABLE ${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX})
        endif()

        message(STATUS "使用 protoc: ${PROTOC_EXECUTABLE}")
        message(STATUS "Proto 源目录: ${CMAKE_SOURCE_DIR}/schema")
        message(STATUS "输出目录: ${GENERATED_PROTO_DIR}")

        # 立即执行 protoc 生成文件
        execute_process(
            COMMAND ${PROTOC_EXECUTABLE}
                --cpp_out=${GENERATED_PROTO_DIR}
                --proto_path=${CMAKE_SOURCE_DIR}/schema
                ${PROTO_FILE_NAMES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/schema
            RESULT_VARIABLE PROTOC_RESULT
            OUTPUT_VARIABLE PROTOC_OUTPUT
            ERROR_VARIABLE PROTOC_ERROR
        )

        if(NOT PROTOC_RESULT EQUAL 0)
            message(FATAL_ERROR "生成 protobuf 文件失败:\n${PROTOC_ERROR}")
        else()
            message(STATUS "Protobuf 文件生成成功")
        endif()
    else()
        message(STATUS "Protobuf 文件已存在，跳过生成")
    endif()

    # 添加手动重新生成目标（用于强制重新生成）
    add_custom_target(regenerate_protos
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${GENERATED_PROTO_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_PROTO_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo "重新配置以生成 proto 文件..."
        COMMAND ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "删除生成的 protobuf 文件并重新配置"
        VERBATIM
    )

    # 添加从源代码重新构建 protobuf 的自定义目标
    add_custom_target(rebuild_protobuf
        COMMAND ${CMAKE_COMMAND} -E echo "从源代码重新构建 protobuf..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/third_party/protobuf-build
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/third_party/protobuf-install
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${GENERATED_PROTO_DIR}
        COMMENT "从源代码重新构建 protobuf（将触发 CMake 重新配置）"
    )

    # 将生成目录添加到包含路径
    include_directories(${GENERATED_PROTO_DIR})

    # 将生成的文件列表传递到父作用域
    set(ALL_PROTO_SRCS ${ALL_PROTO_SRCS} PARENT_SCOPE)
    set(ALL_PROTO_HDRS ${ALL_PROTO_HDRS} PARENT_SCOPE)
    set(GENERATED_PROTO_DIR ${GENERATED_PROTO_DIR} PARENT_SCOPE)

    message(STATUS "Protobuf 文件生成配置完成")
endfunction()
