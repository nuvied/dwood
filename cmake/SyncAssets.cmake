cmake_minimum_required(VERSION 3.10)

if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "Asset directory does not exist: ${SOURCE_DIR}")
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
                "${SOURCE_DIR}" "${DESTINATION_DIR}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR
            "Unable to update assets from ${SOURCE_DIR}. Close any game or editor using them and retry.\n${error}")
    endif()
    return()
endif()

file(GLOB_RECURSE assets LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
foreach(asset IN LISTS assets)
    if(NOT EXISTS "${SOURCE_DIR}/${asset}")
        message(FATAL_ERROR "Cannot enumerate asset ${asset}; filenames containing semicolons require CMake 3.26 or newer.")
    endif()
    get_filename_component(parent "${DESTINATION_DIR}/${asset}" DIRECTORY)
    file(MAKE_DIRECTORY "${parent}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${SOURCE_DIR}/${asset}" "${DESTINATION_DIR}/${asset}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR
            "Unable to update asset ${asset}. Close any game or editor using it and retry.\n${error}")
    endif()
endforeach()
