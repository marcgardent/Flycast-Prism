cmake_minimum_required(VERSION 3.10)
project(benchmarker)

set(CMAKE_CXX_STANDARD 17)

# Attempt to find SDL2 if not already found or target not defined
if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static AND NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

add_executable(benchmarker main.cpp)

# flycast_plugin_api.h is in the parent directory
target_include_directories(benchmarker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

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

message(STATUS "Benchmarker configured with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
