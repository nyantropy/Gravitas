cmake_minimum_required(VERSION 3.10)
project(receiver_project)

# Automatically collect all .cpp files in the directory
file(GLOB SOURCES ./*.cpp ./*.c ./*.h)

# Add the executable
add_executable(receiver ${SOURCES})

# Set the output directory for the executable
set_target_properties(receiver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Find and link libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED libavdevice libavfilter libavformat libavcodec libswresample libswscale libavutil)

# Manually link FFmpeg libraries
target_link_libraries(receiver
    avdevice
    avfilter
    avformat
    avcodec
    swresample
    swscale
    avutil
)

# Include FFmpeg directories
include_directories(${LIBAV_INCLUDE_DIRS})

# List of all dependencies here
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(receiver Vulkan::Vulkan)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
target_link_libraries(receiver ${PNG_LIBRARIES})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
target_link_libraries(receiver ${SDL2_LIBRARIES})

find_package(glm CONFIG REQUIRED)
target_include_directories(receiver PUBLIC ${glm_INCLUDE_DIRS})
target_link_libraries(receiver glm)
