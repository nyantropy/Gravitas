cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#find Vulkan on the system
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")

#GLFW will be compiled as a separate project
include(ExternalProject)
ExternalProject_Add(GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/glfw
    INSTALL_DIR ${CMAKE_BINARY_DIR}/glfw
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

#get GLFW include directories
ExternalProject_Get_Property(GLFW SOURCE_DIR)
set(GLFW_INCLUDE_DIRS ${SOURCE_DIR}/include)

#get GLFW binary directories
ExternalProject_Get_Property(GLFW BINARY_DIR)
set(GLFW_LIB_DIR ${BINARY_DIR}/src)

#source files for the library are all .c .h .cpp files
file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC})
add_dependencies(Gravitas GLFW)

#set all the include directories for this target
target_include_directories(Gravitas PUBLIC 
    ${CMAKE_SOURCE_DIR}/external/glm
    ${GLFW_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

#link all the required libraries
target_link_libraries(Gravitas 
    Vulkan::Vulkan
    $<$<PLATFORM_ID:Windows>:${GLFW_LIB_DIR}/glfw3.lib> #windoof
    $<$<NOT:$<PLATFORM_ID:Windows>>:${GLFW_LIB_DIR}/libglfw3.a> #linux
)

#output dirs
set_target_properties(Gravitas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

#cpack settings
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

#message(STATUS "Finished Building Graphics Engine!")
