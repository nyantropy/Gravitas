cmake_minimum_required(VERSION 3.5.0)

project(GravitasApp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# main target is just our small cpp file
add_executable(main main.cpp)
add_dependencies(main Gravitas)

# Include the graphics engine
target_include_directories(main PUBLIC ${CMAKE_SOURCE_DIR}/graphics_engine)
target_link_libraries(main Gravitas)

# Set working directory for all platforms
set_target_properties(main PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Set the output name to include .exe extension on Windows
if (WIN32)
    set_target_properties(main PROPERTIES OUTPUT_NAME "main.exe")
endif()

# Set working directory for Visual Studio
if (MSVC)
    set_target_properties(main PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
