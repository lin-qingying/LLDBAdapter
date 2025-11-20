# Download and setup LLVM/LLDB automatically

function(download_llvm)
    set(LLVM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llvm-project)
    set(LLVM_REPO_URL "https://gitcode.com/openharmony/third_party_llvm-project.git")

    message(STATUS "Checking for LLVM/LLDB...")

    # Check if LLVM already exists
    if(EXISTS ${LLVM_DIR}/llvm/CMakeLists.txt)
        message(STATUS "Found existing LLVM at ${LLVM_DIR}")
        set(LLVM_SOURCE_DIR ${LLVM_DIR}/llvm PARENT_SCOPE)
        set(LLDB_SOURCE_DIR ${LLVM_DIR}/lldb PARENT_SCOPE)
        set(LLVM_INCLUDE_DIR ${LLVM_DIR}/llvm/include PARENT_SCOPE)
        set(LLDB_INCLUDE_DIR ${LLVM_DIR}/lldb/include PARENT_SCOPE)
        return()
    endif()

    # Find git executable
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "Git not found. Cannot clone LLVM. Git is required for this project.")
        return()
    endif()

    # Create third_party directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

    # Clone LLVM repository with depth 1
    message(STATUS "Cloning LLVM from ${LLVM_REPO_URL}...")
    message(STATUS "This may take several minutes, please wait...")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --depth 1 ${LLVM_REPO_URL} ${LLVM_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        RESULT_VARIABLE GIT_CLONE_RESULT
        OUTPUT_VARIABLE GIT_CLONE_OUTPUT
        ERROR_VARIABLE GIT_CLONE_ERROR
    )

    if(NOT GIT_CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone LLVM: ${GIT_CLONE_ERROR}")
        return()
    endif()

    # Verify the clone was successful
    if(NOT EXISTS ${LLVM_DIR}/llvm/CMakeLists.txt)
        message(FATAL_ERROR "LLVM clone incomplete.")
        return()
    endif()

    message(STATUS "Successfully cloned LLVM to ${LLVM_DIR}")

    # Set the LLVM paths for parent scope
    set(LLVM_SOURCE_DIR ${LLVM_DIR}/llvm PARENT_SCOPE)
    set(LLDB_SOURCE_DIR ${LLVM_DIR}/lldb PARENT_SCOPE)
    set(LLVM_INCLUDE_DIR ${LLVM_DIR}/llvm/include PARENT_SCOPE)
    set(LLDB_INCLUDE_DIR ${LLVM_DIR}/lldb/include PARENT_SCOPE)

    message(STATUS "LLVM include directory: ${LLVM_DIR}/llvm/include")
    message(STATUS "LLDB include directory: ${LLVM_DIR}/lldb/include")
endfunction()