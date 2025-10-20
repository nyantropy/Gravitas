add_library(
    event

    GtsEvent.hpp
    GtsEvent.cpp
)

target_link_libraries(event PUBLIC Gravitas)

target_include_directories(event PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")