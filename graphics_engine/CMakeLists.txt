cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 20)

#project(Gravitas VERSION 0.1.0 LANGUAGES C CXX)

#include(CTest)
#enable_testing()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#find vulkan on the system
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")

#glfw will be compiled as a seperate project, since its binaries are different as well
include(ExternalProject)
ExternalProject_Add(GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/glfw
    INSTALL_DIR ${CMAKE_BINARY_DIR}/glfw
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

#these are the glfw include directories
ExternalProject_Get_Property(GLFW SOURCE_DIR)
set(GLFW_INCLUDE_DIRS ${SOURCE_DIR}/include)

#as well as the binary directories
ExternalProject_Get_Property(GLFW BINARY_DIR)
set(GLFW_LIB_DIR ${BINARY_DIR}/src)

#this will be an actual library in the future, right now it is de facto empty
file(GLOB GravitasSRC ./*.cpp ./*.c ./*.h)
add_library(Gravitas STATIC ${GravitasSRC})
add_dependencies(Gravitas GLFW)


#these are all of the include directories, aka vulkan, glm and of course glfw
target_include_directories(Gravitas PUBLIC 
    ${CMAKE_SOURCE_DIR}/external/glm
    ${GLFW_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

#now we link them against each other
target_link_libraries(Gravitas 
    Vulkan::Vulkan
    ${GLFW_LIB_DIR}/libglfw3.a # Assuming static linking
)

#thats where our library output goes
set_target_properties(Gravitas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)

# CPack settings
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Finished Building Graphics Engine!")