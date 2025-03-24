cmake_minimum_required(VERSION 3.10)
project(devilwood)
set(CMAKE_CXX_STANDARD 17)



file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)



add_executable(devilwood ${SRC_FILES})

target_include_directories(devilwood PRIVATE 

                            ${CMAKE_SOURCE_DIR}/include
                            ${CMAKE_SOURCE_DIR}/src/headers
                            )

target_link_libraries(devilwood PRIVATE ${CMAKE_SOURCE_DIR}/lib/libraylib.a)


file(COPY ${CMAKE_SOURCE_DIR}/resource DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
# file(COPY ${CMAKE_SOURCE_DIR}/src/headers DESTINATION ../../Devilwood_main/app/src/main/cpp)
#file(COPY ${CMAKE_SOURCE_DIR}/src/headers DESTINATION ../../Devilwood_main/app/src/main/cpp)



add_custom_target(copy_files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src"  "E:/raylib_projects/Devilwood_main/app/src/main/cpp"
)

add_dependencies(devilwood copy_files)


add_custom_target(build_android
                   COMMAND cmd /c "gradlew.bat assembleDebug"
                   COMMAND cmd /c "gradlew.bat installDebug"
                   WORKING_DIRECTORY ../../Devilwood_main)

# add_dependencies(build_android)

if (WIN32)
    target_link_libraries(devilwood PRIVATE opengl32 gdi32 winmm)
endif()


if (APPLE)
    target_link_libraries(devilwood PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()