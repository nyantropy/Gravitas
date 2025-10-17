add_library(
    formatutil

    FormatUtil.hpp
    FormatUtil.cpp
)

target_link_libraries(formatutil PUBLIC graphicsutil)

target_include_directories(formatutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")