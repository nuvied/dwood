cmake_minimum_required(VERSION 3.10)
project(devilwood)
set(CMAKE_CXX_STANDARD 17)

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(DEVILWOOD_CUSTOM_FRAME_CONTROL_DEFAULT OFF)
if(DEFINED VCPKG_TARGET_TRIPLET AND raylib_VERSION VERSION_GREATER_EQUAL "6.0")
    set(DEVILWOOD_CUSTOM_FRAME_CONTROL_DEFAULT ON)
endif()
option(DEVILWOOD_CUSTOM_FRAME_CONTROL "Match a raylib build with SUPPORT_CUSTOM_FRAME_CONTROL enabled" ${DEVILWOOD_CUSTOM_FRAME_CONTROL_DEFAULT})

file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/cpp/*.cpp
                    ${CMAKE_SOURCE_DIR}/src/main.cpp

)
list(REMOVE_ITEM SRC_FILES ${CMAKE_SOURCE_DIR}/src/cpp/VariableStore.cpp)
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/Editor/src/SceneObject.cpp)
list(APPEND SRC_FILES ${CMAKE_SOURCE_DIR}/src/cpp/InventoryItems.cpp)

file(GLOB EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/*.cpp 
                     ${CMAKE_SOURCE_DIR}/src/cpp/*.cpp
                     )
list(REMOVE_ITEM EDITOR_SRC ${CMAKE_SOURCE_DIR}/src/cpp/VariableStore.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/include/rlImGui/rlImGui.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/src/SceneEditor.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/src/SoundManagerEditor.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/src/SceneObject.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/src/UIEditor.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/Editor/src/InventoryItemEditor.cpp)
list(APPEND EDITOR_SRC ${CMAKE_SOURCE_DIR}/src/cpp/InventoryItems.cpp)

add_library(scene_document STATIC ${CMAKE_SOURCE_DIR}/Editor/src/SceneDocument.cpp
                                  ${CMAKE_SOURCE_DIR}/src/cpp/VariableStore.cpp)
target_include_directories(scene_document PUBLIC ${CMAKE_SOURCE_DIR}/Editor/include
                                                 ${CMAKE_SOURCE_DIR}/src/headers)
target_link_libraries(scene_document PUBLIC nlohmann_json::nlohmann_json)

add_executable(editor ${EDITOR_SRC})
target_compile_definitions(editor PRIVATE
    RLIMGUIAPI=
    DEVILWOOD_SCENES_DIR="${CMAKE_SOURCE_DIR}/scenes"
    DEVILWOOD_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
    DEVILWOOD_RESOURCE_TEMPLATE_DIR="${CMAKE_BINARY_DIR}/resource"
    DEVILWOOD_SHADER_TEMPLATE_DIR="${CMAKE_BINARY_DIR}/shaders")


target_include_directories(editor PRIVATE ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/src/headers
 ${CMAKE_SOURCE_DIR}/Editor/include/rlImGui
 ${CMAKE_SOURCE_DIR}/Editor/include/rlImGui/extras


)



add_executable(devilwood ${SRC_FILES})
target_compile_definitions(devilwood PRIVATE
    DEVILWOOD_SCENES_DIR="${CMAKE_SOURCE_DIR}/scenes"
    DEVILWOOD_PROJECT_DIR="${CMAKE_SOURCE_DIR}")


if(DEVILWOOD_CUSTOM_FRAME_CONTROL)
    target_compile_definitions(devilwood PRIVATE DEVILWOOD_CUSTOM_FRAME_CONTROL)
    target_compile_definitions(editor PRIVATE DEVILWOOD_CUSTOM_FRAME_CONTROL)
endif()

target_include_directories(devilwood PRIVATE 

                            ${CMAKE_SOURCE_DIR}/include
                            ${CMAKE_SOURCE_DIR}/src/headers
                            ${CMAKE_SOURCE_DIR}/Editor/include
                            )

target_link_libraries(devilwood PRIVATE raylib glfw scene_document nlohmann_json::nlohmann_json)
target_link_libraries(editor PRIVATE raylib glfw imgui::imgui unofficial::imgui-node-editor::imgui-node-editor scene_document)

include(CTest)
if(BUILD_TESTING)
    add_executable(scene_document_tests tests/scene_document_tests.cpp)
    target_link_libraries(scene_document_tests PRIVATE scene_document)
    add_test(NAME scene_document_tests COMMAND scene_document_tests)
    file(GLOB GAME_TEST_SOURCES ${CMAKE_SOURCE_DIR}/src/cpp/*.cpp)
    list(REMOVE_ITEM GAME_TEST_SOURCES ${CMAKE_SOURCE_DIR}/src/cpp/VariableStore.cpp)
    add_executable(scene_object_tests tests/scene_object_tests.cpp
                   Editor/src/SceneObject.cpp src/cpp/InventoryItems.cpp ${GAME_TEST_SOURCES})
    target_include_directories(scene_object_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/headers ${CMAKE_SOURCE_DIR}/Editor/include)
    target_link_libraries(scene_object_tests PRIVATE raylib glfw scene_document nlohmann_json::nlohmann_json)
    if(WIN32)
        target_link_libraries(scene_object_tests PRIVATE opengl32 gdi32 winmm)
    elseif(APPLE)
        target_link_libraries(scene_object_tests PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    endif()
    add_test(NAME scene_object_tests COMMAND scene_object_tests)

    add_executable(rlimgui_input_tests tests/rlimgui_input_tests.cpp)
    target_compile_definitions(rlimgui_input_tests PRIVATE RLIMGUIAPI=)
    target_include_directories(rlimgui_input_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/Editor/include/rlImGui)
    target_link_libraries(rlimgui_input_tests PRIVATE raylib glfw imgui::imgui)
    if(WIN32)
        target_link_libraries(rlimgui_input_tests PRIVATE opengl32 gdi32 winmm)
    elseif(APPLE)
        target_link_libraries(rlimgui_input_tests PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    endif()
    add_test(NAME rlimgui_input_tests COMMAND rlimgui_input_tests)
endif()


if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    add_custom_target(sync_resources
        COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}/resource"
                "-DDESTINATION_DIR=${CMAKE_BINARY_DIR}/resource"
                -P "${CMAKE_SOURCE_DIR}/cmake/SyncAssets.cmake"
        VERBATIM)
    add_custom_target(sync_shaders
        COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}/shaders"
                "-DDESTINATION_DIR=${CMAKE_BINARY_DIR}/shaders"
                -P "${CMAKE_SOURCE_DIR}/cmake/SyncAssets.cmake"
        VERBATIM)
    add_dependencies(editor sync_shaders)
    add_dependencies(devilwood sync_shaders)
    add_dependencies(editor sync_resources)
    add_dependencies(devilwood sync_resources)
endif()
# file(COPY ${CMAKE_SOURCE_DIR}/src/headers DESTINATION ../../Devilwood_main/app/src/main/cpp)
#file(COPY ${CMAKE_SOURCE_DIR}/src/headers DESTINATION ../../Devilwood_main/app/src/main/cpp)



# add_custom_target(copy_files ALL
#     COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src"  "E:/raylib_projects/Devilwood_main/app/src/main/cpp"
#      COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src/cpp"  "E:/raylib_projects/Devilwood_main/app/src/main/cpp"
# )

# add_dependencies(devilwood copy_files)


add_custom_target(build_android
                   COMMAND cmd /c "gradlew.bat assembleDebug"
                   COMMAND cmd /c "gradlew.bat installDebug"
                   WORKING_DIRECTORY ../../Devilwood_main)

# add_dependencies(build_android)

if (WIN32)
    target_link_libraries(devilwood PRIVATE opengl32 gdi32 winmm)
    target_link_libraries(editor PRIVATE opengl32 gdi32 winmm ole32 shell32 uuid)
endif()


if (APPLE)
    target_link_libraries(devilwood PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    target_link_libraries(editor PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()