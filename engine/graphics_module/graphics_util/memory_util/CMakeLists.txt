add_library(
    memoryutil

    MemoryUtil.hpp
    MemoryUtil.cpp
)

target_link_libraries(memoryutil PUBLIC graphicsutil)

target_include_directories(memoryutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")