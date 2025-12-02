# 从本地third_party目录编译protobuf静态库（不使用系统库）
cmake_minimum_required(VERSION 3.16)

function(build_protobuf)
    set(PROTOBUF_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-build)
    set(PROTOBUF_INSTALL_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-install)
    set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)

    message(STATUS "从本地源码构建protobuf: ${PROTOBUF_SOURCE_DIR}")

    # 检查是否已经构建过，避免重复编译
    set(PROTOBUF_LIB_FOUND FALSE)
    set(PROTOBUF_LIB_PATH "")
    set(PROTOC_LIB_PATH "")

    # 检查 protobuf 库文件
    if(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.lib)
        set(PROTOBUF_LIB_FOUND TRUE)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.lib)
        set(PROTOBUF_LIB_FOUND TRUE)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
        set(PROTOBUF_LIB_FOUND TRUE)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.a)
        set(PROTOBUF_LIB_FOUND TRUE)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.a)
    endif()

    # 检查 protoc 库文件
    if(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.lib)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.lib)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.a)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.a)
    endif()

    if(PROTOBUF_LIB_FOUND)
        message(STATUS "发现已有的protobuf构建: ${PROTOBUF_INSTALL_DIR}")

        # 查找所有absl库文件
        file(GLOB ABSL_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libabsl_*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/absl_*.lib"
        )

        # 查找utf8相关库（优先使用utf8_validity以避免重复符号）
        file(GLOB ALL_UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libutf8_validity.a"
                "${PROTOBUF_INSTALL_DIR}/lib/utf8_validity.lib"
        )

        if(NOT ALL_UTF8_LIBRARIES)
            # 如果找不到utf8_validity，回退到utf8_range
            file(GLOB ALL_UTF8_LIBRARIES
                    LIST_DIRECTORIES false
                    "${PROTOBUF_INSTALL_DIR}/lib/libutf8_range.a"
                    "${PROTOBUF_INSTALL_DIR}/lib/utf8_range.lib"
            )
        endif()

        # 查找upb相关库
        file(GLOB UPB_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libupb*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/upb*.lib"
        )

        if(ABSL_LIBRARIES)
            list(LENGTH ABSL_LIBRARIES ABSL_COUNT)
            message(STATUS "在现有构建中发现 ${ABSL_COUNT} 个absl库")
        endif()

        if(ALL_UTF8_LIBRARIES)
            list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
            message(STATUS "在现有构建中发现 ${UTF8_COUNT} 个utf8库")
        endif()

        if(UPB_LIBRARIES)
            list(LENGTH UPB_LIBRARIES UPB_COUNT)
            message(STATUS "在现有构建中发现 ${UPB_COUNT} 个upb库")
        endif()

        # 设置protobuf变量
        set(PROTOBUF_INCLUDE_DIRS
                ${PROTOBUF_INSTALL_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src
                PARENT_SCOPE
        )
        set(PROTOBUF_LIBRARIES
                ${PROTOBUF_LIB_PATH}
                ${PROTOC_LIB_PATH}
                ${ABSL_LIBRARIES}
                ${ALL_UTF8_LIBRARIES}
                ${UPB_LIBRARIES}
                PARENT_SCOPE
        )
        set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX} PARENT_SCOPE)

        return()
    endif()

    # 验证本地protobuf源代码存在
    if(NOT EXISTS ${PROTOBUF_SOURCE_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "在 ${PROTOBUF_SOURCE_DIR} 未找到protobuf源码。请确保protobuf位于third_party/protobuf目录中。")
    endif()

    # 确保子模块已初始化（如果有的话）
    if(EXISTS ${PROTOBUF_SOURCE_DIR}/.gitmodules)
        message(STATUS "初始化protobuf子模块...")
        find_program(GIT_EXECUTABLE git)
        if(GIT_EXECUTABLE)
            execute_process(
                    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${PROTOBUF_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMODULE_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET
            )

            if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
                message(WARNING "无法初始化protobuf子模块，这可能导致构建问题")
            endif()
        endif()
    endif()

    # 创建构建目录
    file(MAKE_DIRECTORY ${PROTOBUF_BUILD_DIR})

    # 配置protobuf构建
    message(STATUS "配置protobuf构建（仅静态链接）...")
    set(PROTOBUF_CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_DIR}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -Dprotobuf_BUILD_TESTS=OFF
            -Dprotobuf_BUILD_EXAMPLES=OFF
            -Dprotobuf_BUILD_PROTOC_BINARIES=ON
            -Dprotobuf_BUILD_SHARED_LIBS=OFF
            -Dprotobuf_WITH_ZLIB=OFF
            -Dprotobuf_WITH_RTTI=ON
            -Dprotobuf_DISABLE_RTTI=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -Dprotobuf_BUILD_LIBPROTOBUF=ON
            -Dprotobuf_BUILD_LIBPROTOBUF_LITE=ON
            -Dprotobuf_BUILD_CONFORMANCE=OFF
            -Dprotobuf_INSTALL=ON
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
    )

    # 如果是Windows，添加额外的配置
    if(WIN32)
        list(APPEND PROTOBUF_CMAKE_ARGS
                -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
                -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        )
    endif()

    # 如果是GCC/Clang，添加编译选项
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND PROTOBUF_CMAKE_ARGS
                -DCMAKE_CXX_FLAGS="-Wno-unused-parameter -Wno-sign-compare"
        )
    endif()

    execute_process(
            COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} ${PROTOBUF_CMAKE_ARGS} ${PROTOBUF_SOURCE_DIR}
            WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_CONFIGURE_RESULT
    )

    if(NOT PROTOBUF_CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "配置protobuf失败")
    endif()

    # 构建protobuf
    message(STATUS "构建protobuf中（这可能需要几分钟）...")
    execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install --parallel
            WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
            RESULT_VARIABLE PROTOBUF_BUILD_RESULT
    )

    if(NOT PROTOBUF_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "构建protobuf失败")
    endif()

    message(STATUS "Protobuf构建成功完成")

    # 设置protobuf变量
    set(PROTOBUF_INCLUDE_DIRS
            ${PROTOBUF_INSTALL_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src
            PARENT_SCOPE
    )

    # 查找实际生成的库文件
    set(PROTOBUF_LIB_PATH "")
    set(PROTOC_LIB_PATH "")

    # 检查 protobuf 库文件
    if(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.lib)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.lib)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.a)
        set(PROTOBUF_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotobufd.a)
    endif()

    # 检查 protoc 库文件
    if(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.lib)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.lib)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.lib)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
    elseif(EXISTS ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.a)
        set(PROTOC_LIB_PATH ${PROTOBUF_INSTALL_DIR}/lib/libprotocd.a)
    endif()

    # 查找所有absl库文件
    file(GLOB ABSL_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/libabsl_*.a"
            "${PROTOBUF_INSTALL_DIR}/lib/absl_*.lib"
    )

    # 查找utf8相关库（优先使用utf8_validity）
    file(GLOB ALL_UTF8_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/libutf8_validity.a"
            "${PROTOBUF_INSTALL_DIR}/lib/utf8_validity.lib"
    )

    if(NOT ALL_UTF8_LIBRARIES)
        # 如果找不到utf8_validity，回退到utf8_range
        file(GLOB ALL_UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libutf8_range.a"
                "${PROTOBUF_INSTALL_DIR}/lib/utf8_range.lib"
        )
    endif()

    # 查找upb相关库
    file(GLOB UPB_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/libupb*.a"
            "${PROTOBUF_INSTALL_DIR}/lib/upb*.lib"
    )

    if(ABSL_LIBRARIES)
        list(LENGTH ABSL_LIBRARIES ABSL_COUNT)
        message(STATUS "发现 ${ABSL_COUNT} 个absl库")
    else()
        message(STATUS "在 ${PROTOBUF_INSTALL_DIR}/lib 中未找到absl库")
    endif()

    if(ALL_UTF8_LIBRARIES)
        list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
        message(STATUS "发现 ${UTF8_COUNT} 个utf8库: ${ALL_UTF8_LIBRARIES}")
    else()
        message(WARNING "未找到utf8库")
    endif()

    if(UPB_LIBRARIES)
        list(LENGTH UPB_LIBRARIES UPB_COUNT)
        message(STATUS "发现 ${UPB_COUNT} 个upb库")
    endif()

    # 设置库文件变量
    set(PROTOBUF_LIBRARIES
            ${PROTOBUF_LIB_PATH}
            ${PROTOC_LIB_PATH}
            ${ABSL_LIBRARIES}
            ${ALL_UTF8_LIBRARIES}
            ${UPB_LIBRARIES}
            PARENT_SCOPE
    )

    set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX} PARENT_SCOPE)

    # 验证protobuf构建
    if(EXISTS ${PROTOBUF_PROTOC_EXECUTABLE})
        execute_process(
                COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --version
                RESULT_VARIABLE PROTOC_TEST_RESULT
                OUTPUT_VARIABLE PROTOC_VERSION_OUTPUT
                ERROR_VARIABLE PROTOC_VERSION_ERROR
        )

        if(PROTOC_TEST_RESULT EQUAL 0)
            message(STATUS "Protobuf版本: ${PROTOC_VERSION_OUTPUT}")
        else()
            message(WARNING "找到Protoc可执行文件但版本检查失败: ${PROTOC_VERSION_ERROR}")
        endif()
    else()
        message(WARNING "在预期位置未找到Protoc可执行文件: ${PROTOBUF_PROTOC_EXECUTABLE}")
    endif()
endfunction()