cmake_minimum_required(VERSION 3.16)
project(DiSketch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译子模块
add_subdirectory(PcapPlusPlus-25.05)
add_subdirectory(SketchLib)

add_library(disketch STATIC
    src/PacketParser.cpp
)

target_include_directories(disketch
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/SketchLib/include
)

# 链接子模块
target_link_libraries(disketch
    PUBLIC
        Packet++
)

# 编译示例
option(DISKETCH_BUILD_EXAMPLES "Build DiSketch examples" ON)

if(DISKETCH_BUILD_EXAMPLES)
    add_executable(parse_pcap examples/parse_pcap.cpp)
    target_link_libraries(parse_pcap PRIVATE disketch)
endif()
