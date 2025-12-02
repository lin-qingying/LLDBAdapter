# 编译本地third_party目录中的protobuf
cmake_minimum_required(VERSION 3.16)

function(build_protobuf)
    # First, try to find system protobuf STATIC libraries only
    set(Protobuf_USE_STATIC_LIBS ON)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)  # Force static library search on Unix

    find_package(Protobuf QUIET)

    if(Protobuf_FOUND)
        # Verify that we found static libraries
        set(HAS_STATIC_LIBS TRUE)
        foreach(lib ${Protobuf_LIBRARIES})
            if(lib MATCHES "\\.so$" OR lib MATCHES "\\.dylib$" OR lib MATCHES "\\.dll$")
                set(HAS_STATIC_LIBS FALSE)
                message(STATUS "Found dynamic protobuf library: ${lib}, will build from source for static linking")
                break()
            endif()
        endforeach()

        if(HAS_STATIC_LIBS)
            message(STATUS "Using system static protobuf: ${Protobuf_VERSION}")
            message(STATUS "  Protoc: ${Protobuf_PROTOC_EXECUTABLE}")
            message(STATUS "  Include dirs: ${Protobuf_INCLUDE_DIRS}")
            message(STATUS "  Libraries: ${Protobuf_LIBRARIES}")

            set(PROTOBUF_INCLUDE_DIRS ${Protobuf_INCLUDE_DIRS} PARENT_SCOPE)
            set(PROTOBUF_LIBRARIES ${Protobuf_LIBRARIES} PARENT_SCOPE)
            set(PROTOBUF_PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE} PARENT_SCOPE)
            return()
        endif()
    endif()

    message(STATUS "System static protobuf not found, will build from source...")

    set(PROTOBUF_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-build)
    set(PROTOBUF_INSTALL_DIR ${CMAKE_BINARY_DIR}/third_party/protobuf-install)
    set(PROTOBUF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf)

    message(STATUS "Using local protobuf source from: ${PROTOBUF_SOURCE_DIR}")

    # 检查是否已经构建过 - 检查两种可能的库文件格式
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
        message(STATUS "Found existing protobuf build at ${PROTOBUF_INSTALL_DIR}")

        # 查找所有absl库文件
        file(GLOB ABSL_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/libabsl_*.a"
            "${PROTOBUF_INSTALL_DIR}/lib/absl_*.lib"
        )

        # 查找utf8_range库文件
        find_library(UTF8_RANGE_LIBRARY
            NAMES utf8_range libutf8_range utf8_ranged libutf8_ranged
            PATHS ${PROTOBUF_INSTALL_DIR}/lib
            NO_DEFAULT_PATH
        )

        if(ABSL_LIBRARIES)
            list(LENGTH ABSL_LIBRARIES ABSL_COUNT)
            message(STATUS "Found ${ABSL_COUNT} absl libraries in existing build")
        else()
            message(STATUS "No absl libraries found in existing protobuf build")
        endif()

        if(UTF8_RANGE_LIBRARY)
            message(STATUS "Found utf8_range library: ${UTF8_RANGE_LIBRARY}")
        else()
            message(STATUS "utf8_range library not found, searching for alternatives...")
            file(GLOB UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/*utf8*.a"
                "${PROTOBUF_INSTALL_DIR}/lib/*utf8*.lib"
            )
            if(UTF8_LIBRARIES)
                message(STATUS "Found utf8 libraries: ${UTF8_LIBRARIES}")
                set(UTF8_RANGE_LIBRARY ${UTF8_LIBRARIES})
            endif()
        endif()

        # Find all utf8-related libraries (utf8_range, utf8_validity, etc.)
        # Note: libutf8_range and libutf8_validity may have duplicate symbols, prefer utf8_validity
        file(GLOB ALL_UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libutf8_validity.a"
                "${PROTOBUF_INSTALL_DIR}/lib/utf8_validity.lib"
        )

        if(ALL_UTF8_LIBRARIES)
            list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
            message(STATUS "Found ${UTF8_COUNT} utf8 libraries: ${ALL_UTF8_LIBRARIES}")
        else()
            # Fallback to utf8_range if validity not found
            file(GLOB ALL_UTF8_LIBRARIES
                    LIST_DIRECTORIES false
                    "${PROTOBUF_INSTALL_DIR}/lib/libutf8_range.a"
                    "${PROTOBUF_INSTALL_DIR}/lib/utf8_range.lib"
            )
            if(ALL_UTF8_LIBRARIES)
                list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
                message(STATUS "Found ${UTF8_COUNT} utf8 libraries (fallback): ${ALL_UTF8_LIBRARIES}")
            endif()
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
        endif()

        # 设置protobuf变量 - 包含protobuf、absl、utf8和upb库
        # 包含构建后的头文件和源码目录中的头文件
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
        message(FATAL_ERROR "Protobuf source not found at ${PROTOBUF_SOURCE_DIR}. Please ensure protobuf is available in third_party/protobuf directory.")
    endif()

    # 确保子模块已初始化（如果有的话）
    if(EXISTS ${PROTOBUF_SOURCE_DIR}/.gitmodules)
        message(STATUS "Initializing protobuf submodules...")
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
                message(WARNING "Failed to initialize protobuf submodules, this might cause build issues")
            endif()
        endif()
    endif()

    # 创建构建目录
    file(MAKE_DIRECTORY ${PROTOBUF_BUILD_DIR})

    # 配置protobuf构建
    message(STATUS "Configuring protobuf build with static linking only...")
    # 强制只构建静态库，确保静态链接
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
        message(FATAL_ERROR "Failed to configure protobuf")
    endif()

    # 构建protobuf
    message(STATUS "Building protobuf (this may take several minutes)...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install --parallel
        WORKING_DIRECTORY ${PROTOBUF_BUILD_DIR}
        RESULT_VARIABLE PROTOBUF_BUILD_RESULT
    )

    if(NOT PROTOBUF_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build protobuf")
    endif()

    message(STATUS "Protobuf build completed successfully")

    # 设置protobuf变量 - 智能检测生成的库文件格式
    # 包含构建后的头文件和源码目录中的头文件
    set(PROTOBUF_INCLUDE_DIRS
        ${PROTOBUF_INSTALL_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/src
        PARENT_SCOPE
    )

    # 查找实际生成的 protobuf 库文件
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

    # 查找utf8_range库文件
    find_library(UTF8_RANGE_LIBRARY
        NAMES utf8_range libutf8_range utf8_ranged libutf8_ranged
        PATHS ${PROTOBUF_INSTALL_DIR}/lib
        NO_DEFAULT_PATH
    )

    if(ABSL_LIBRARIES)
        list(LENGTH ABSL_LIBRARIES ABSL_COUNT)
        message(STATUS "Found ${ABSL_COUNT} absl libraries")
    else()
        message(STATUS "No absl libraries found in ${PROTOBUF_INSTALL_DIR}/lib")
    endif()

    if(UTF8_RANGE_LIBRARY)
        message(STATUS "Found utf8_range library: ${UTF8_RANGE_LIBRARY}")
    else()
        message(STATUS "utf8_range library not found, searching for alternatives...")
        file(GLOB UTF8_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/*utf8*.a"
            "${PROTOBUF_INSTALL_DIR}/lib/*utf8*.lib"
        )
        if(UTF8_LIBRARIES)
            message(STATUS "Found utf8 libraries: ${UTF8_LIBRARIES}")
            set(UTF8_RANGE_LIBRARY ${UTF8_LIBRARIES})
        endif()
    endif()

    # Find all utf8-related libraries (utf8_range, utf8_validity, etc.)
    # Note: libutf8_range and libutf8_validity may have duplicate symbols, prefer utf8_validity
    file(GLOB ALL_UTF8_LIBRARIES
            LIST_DIRECTORIES false
            "${PROTOBUF_INSTALL_DIR}/lib/libutf8_validity.a"
            "${PROTOBUF_INSTALL_DIR}/lib/utf8_validity.lib"
    )

    if(ALL_UTF8_LIBRARIES)
        list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
        message(STATUS "Found ${UTF8_COUNT} utf8 libraries: ${ALL_UTF8_LIBRARIES}")
    else()
        # Fallback to utf8_range if validity not found
        file(GLOB ALL_UTF8_LIBRARIES
                LIST_DIRECTORIES false
                "${PROTOBUF_INSTALL_DIR}/lib/libutf8_range.a"
                "${PROTOBUF_INSTALL_DIR}/lib/utf8_range.lib"
        )
        if(ALL_UTF8_LIBRARIES)
            list(LENGTH ALL_UTF8_LIBRARIES UTF8_COUNT)
            message(STATUS "Found ${UTF8_COUNT} utf8 libraries (fallback): ${ALL_UTF8_LIBRARIES}")
        endif()
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
    endif()

    # 设置库文件变量 - 包含protobuf、absl、utf8和upb库
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
            message(STATUS "Protobuf version: ${PROTOC_VERSION_OUTPUT}")
        else()
            message(WARNING "Protoc executable found but version check failed: ${PROTOC_VERSION_ERROR}")
        endif()
    else()
        message(WARNING "Protoc executable not found at expected location: ${PROTOBUF_PROTOC_EXECUTABLE}")
    endif()
endfunction()