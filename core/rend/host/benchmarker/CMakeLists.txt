cmake_minimum_required(VERSION 3.10)
project(benchmarker)

set(CMAKE_CXX_STANDARD 17)

# Attempt to find SDL2 if not already found or target not defined
if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static AND NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/imgui)

add_executable(benchmarker main.cpp TestCase.h TestManager.h
        cases/TestGEO01.h
        cases/TestGEO02.h
        cases/TestSHD01.h
        cases/TestTEX01.h
        cases/TestCommon.h
        cases/TestGEO03.h
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_cjk.cpp
)

# flycast_plugin_api.h is in the parent directory
target_include_directories(benchmarker PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${IMGUI_DIR}
)

if(TARGET SDL2::SDL2-static)
    target_link_libraries(benchmarker PRIVATE SDL2::SDL2-static)
elseif(TARGET SDL2::SDL2)
    target_link_libraries(benchmarker PRIVATE SDL2::SDL2)
elseif(SDL2_FOUND)
    target_include_directories(benchmarker PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(benchmarker PRIVATE ${SDL2_LIBRARIES})
endif()

# Link against dynamic loading library (dl on Linux)
target_link_libraries(benchmarker PRIVATE ${CMAKE_DL_LIBS})

# Define loader for imgui_impl_opengl3
target_compile_definitions(benchmarker PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

message(STATUS "Benchmarker configured with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
