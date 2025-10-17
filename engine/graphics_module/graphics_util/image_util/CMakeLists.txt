add_library(
    imageutil

    ImageUtil.hpp
    ImageUtil.cpp
)

target_link_libraries(imageutil PUBLIC graphicsutil)

target_include_directories(imageutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")