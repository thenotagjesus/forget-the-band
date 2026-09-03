if(EXISTS "${src}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${src}" "${dst}")
endif()
