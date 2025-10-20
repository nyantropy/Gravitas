add_library(
    windowmanager
    
    WindowManager.hpp
    WindowManager.cpp

    WindowManagerConfig.h
)

target_include_directories(windowmanager PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(windowmanager PUBLIC output)