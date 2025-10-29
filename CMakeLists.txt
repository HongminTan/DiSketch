cmake_minimum_required(VERSION 3.16)
project(DiSketch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json 文件给 LSP 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认构建类型为 Release，可通过 -DCMAKE_BUILD_TYPE=Debug 切换
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 开启调试选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in Debug mode")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# 编译子模块
add_subdirectory(PcapPlusPlus-25.05)
add_subdirectory(SketchLib)

file(GLOB_RECURSE DISKETCH_SOURCES "src/*.cpp")

add_library(disketch STATIC ${DISKETCH_SOURCES})

# Release 模式下对 DiSketch 开启严格警告
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(disketch PRIVATE -O3 -DNDEBUG -Wall -Wextra)
endif()

target_include_directories(disketch
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/SketchLib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/simpleini
)

# 链接子模块
target_link_libraries(disketch
    PUBLIC
        Packet++
        SketchLib
)

# 编译示例
option(DISKETCH_BUILD_EXAMPLES "Build DiSketch examples" ON)

if(DISKETCH_BUILD_EXAMPLES)
    add_executable(parse_pcap examples/parse_pcap.cpp)
    target_link_libraries(parse_pcap PRIVATE disketch)

    add_executable(baseline examples/baseline.cpp)
    target_link_libraries(baseline PRIVATE disketch SketchLib)

    add_executable(disketch_simulator examples/disketch_simulator.cpp)
    target_link_libraries(disketch_simulator PRIVATE disketch SketchLib)
endif()
