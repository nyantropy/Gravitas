add_library(
    options INTERFACE

    GraphicsOptions.h
    GraphicsConstants.h

)

target_include_directories(options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")