cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

project(Gravitas VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

add_subdirectory(engine)
add_subdirectory(applications)
