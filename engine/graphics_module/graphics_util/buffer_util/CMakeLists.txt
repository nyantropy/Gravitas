add_library(
    bufferutil

    BufferUtil.hpp
    BufferUtil.cpp
)

target_link_libraries(bufferutil PUBLIC graphicsutil Gravitas)

target_include_directories(bufferutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")