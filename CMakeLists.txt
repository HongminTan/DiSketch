cmake_minimum_required(VERSION 3.16)
project(DiSketch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译子模块
add_subdirectory(PcapPlusPlus-25.05)
add_subdirectory(SketchLib)

file(GLOB_RECURSE DISKETCH_SOURCES "src/*.cpp")

add_library(disketch STATIC ${DISKETCH_SOURCES})

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

    add_executable(disketch_simulation examples/disketch_simulation.cpp)
    target_link_libraries(disketch_simulation PRIVATE disketch SketchLib)
endif()
