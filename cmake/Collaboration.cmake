function(mindarchy_add_collaboration_compat_test)
    enable_language(C)
    set(AUTOMERGE_ROOT "" CACHE PATH "Root containing the pinned Automerge C header and library")
    find_path(AUTOMERGE_INCLUDE_DIR automerge.h
        HINTS "${AUTOMERGE_ROOT}" "$ENV{AUTOMERGE_ROOT}")
    find_library(AUTOMERGE_LIBRARY
        NAMES automerge_core_linux_amd64 automerge_core
        HINTS "${AUTOMERGE_ROOT}/deps" "${AUTOMERGE_ROOT}")

    if(NOT AUTOMERGE_INCLUDE_DIR OR NOT AUTOMERGE_LIBRARY)
        message(FATAL_ERROR "MINDARCHY_BUILD_COLLABORATION_COMPAT requires AUTOMERGE_ROOT with automerge.h and libautomerge_core")
    endif()

    add_executable(collaboration_compat tests/collaboration_compat.cpp tests/collaboration_compat_shim.c)
    target_include_directories(collaboration_compat PRIVATE "${AUTOMERGE_INCLUDE_DIR}")
    target_link_libraries(collaboration_compat PRIVATE "${AUTOMERGE_LIBRARY}" m)
    add_test(NAME collaboration_compat COMMAND collaboration_compat)
endfunction()
