# ============================================
# Protobuf 目标平台构建模块（交叉编译）
# ============================================
# 此模块负责为目标平台构建 protobuf 库
# 使用宿主机的 protoc 工具生成 proto 文件
# ============================================

cmake_minimum_required(VERSION 3.16)

function(build_protobuf_target PROTOC_EXECUTABLE_HOST)
    message(STATUS "====================================")
    message(STATUS "设置 Protobuf 目标平台构建配置...")
    message(STATUS "====================================")

    # 设置 protobuf 构建和安装路径（目标平台）
    set(PROTOBUF_TARGET_BUILD_DIR ${CMAKE_BINARY_DIR}/target/protobuf-build)
    set(PROTOBUF_TARGET_INSTALL_DIR ${CMAKE_BINARY_DIR}/target/protobuf-install)
    set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)
    set(PROTOBUF_TARGET_CONFIG_FILE ${PROTOBUF_TARGET_INSTALL_DIR}/lib/cmake/protobuf/protobuf-config.cmake)

    # 检查是否需要构建 protobuf
    if(NOT EXISTS ${PROTOBUF_TARGET_CONFIG_FILE})
        message(STATUS "目标平台 Protobuf 未找到，开始交叉编译...")

        # 验证源代码存在
        if(NOT EXISTS ${PROTOBUF_SOURCE_DIR}/CMakeLists.txt)
            message(FATAL_ERROR "在 ${PROTOBUF_SOURCE_DIR} 未找到 Protobuf 源代码")
        endif()

        # 创建构建目录
        file(MAKE_DIRECTORY ${PROTOBUF_TARGET_BUILD_DIR})

        # 配置 protobuf 构建（使用交叉编译工具链）
        message(STATUS "配置目标平台 Protobuf 交叉编译...")
        message(STATUS "  目标系统: ${CMAKE_SYSTEM_NAME}")
        message(STATUS "  目标架构: ${CMAKE_SYSTEM_PROCESSOR}")
        message(STATUS "  使用宿主机 protoc: ${PROTOC_EXECUTABLE_HOST}")

        execute_process(
            COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR}
                -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                -DCMAKE_INSTALL_PREFIX=${PROTOBUF_TARGET_INSTALL_DIR}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_CXX_STANDARD=17
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -Dprotobuf_BUILD_TESTS=OFF
                -Dprotobuf_BUILD_EXAMPLES=OFF
                -Dprotobuf_BUILD_PROTOC_BINARIES=OFF
                -Dprotobuf_PROTOC_EXE=${PROTOC_EXECUTABLE_HOST}
                -Dprotobuf_BUILD_SHARED_LIBS=OFF
                -Dprotobuf_WITH_ZLIB=OFF
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -Dprotobuf_INSTALL=ON
                ${PROTOBUF_SOURCE_DIR}
            WORKING_DIRECTORY ${PROTOBUF_TARGET_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_CONFIGURE_RESULT
        )

        if(NOT PROTOBUF_CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "配置目标平台 Protobuf 失败")
        endif()

        # 构建 protobuf（仅构建库，不构建 protoc）
        message(STATUS "构建目标平台 Protobuf 库...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install --parallel
            WORKING_DIRECTORY ${PROTOBUF_TARGET_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_BUILD_RESULT
        )

        if(NOT PROTOBUF_BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "构建目标平台 Protobuf 失败")
        endif()

        message(STATUS "目标平台 Protobuf 构建并安装成功到: ${PROTOBUF_TARGET_INSTALL_DIR}")
    else()
        message(STATUS "找到已构建的目标平台 Protobuf: ${PROTOBUF_TARGET_CONFIG_FILE}")
    endif()

    # 使用 find_package 查找已安装的 protobuf
    set(CMAKE_PREFIX_PATH ${PROTOBUF_TARGET_INSTALL_DIR} ${CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)

    # 查找 protobuf 包
    find_package(protobuf CONFIG REQUIRED)

    # 显示 protobuf 配置信息
    message(STATUS "====================================")
    message(STATUS "目标平台 Protobuf 配置信息:")
    message(STATUS "  版本: ${protobuf_VERSION}")
    message(STATUS "  包含目录: ${Protobuf_INCLUDE_DIRS}")
    message(STATUS "  安装目录: ${PROTOBUF_TARGET_INSTALL_DIR}")
    message(STATUS "====================================")

    # 添加 protobuf 包含目录
    include_directories(${Protobuf_INCLUDE_DIRS})

    # ============================================
    # 使用宿主机 protoc 生成 Proto 文件
    # ============================================
    message(STATUS "设置 protobuf 文件生成...")

    # Proto 文件列表
    set(PROTO_FILE_NAMES
        event.proto
        model.proto
        request.proto
        response.proto
    )

    # 完整的 Proto 文件路径
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
    foreach(PROTO_SRC ${ALL_PROTO_SRCS})
        if(NOT EXISTS ${PROTO_SRC})
            set(NEED_REGENERATE TRUE)
            message(STATUS "Proto 文件不存在，需要生成: ${PROTO_SRC}")
            break()
        endif()
    endforeach()

    # 使用宿主机 protoc 生成文件
    if(NEED_REGENERATE)
        message(STATUS "使用宿主机 protoc 生成 protobuf 文件...")
        message(STATUS "  protoc: ${PROTOC_EXECUTABLE_HOST}")
        message(STATUS "  Proto 源目录: ${CMAKE_SOURCE_DIR}/schema")
        message(STATUS "  输出目录: ${GENERATED_PROTO_DIR}")

        execute_process(
            COMMAND ${PROTOC_EXECUTABLE_HOST}
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

    # 将生成目录添加到包含路径
    include_directories(${GENERATED_PROTO_DIR})

    # 将生成的文件列表传递到父作用域
    set(ALL_PROTO_SRCS ${ALL_PROTO_SRCS} PARENT_SCOPE)
    set(ALL_PROTO_HDRS ${ALL_PROTO_HDRS} PARENT_SCOPE)
    set(GENERATED_PROTO_DIR ${GENERATED_PROTO_DIR} PARENT_SCOPE)

    message(STATUS "目标平台 Protobuf 文件生成配置完成")
endfunction()