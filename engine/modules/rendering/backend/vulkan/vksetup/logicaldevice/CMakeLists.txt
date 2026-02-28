file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(logicaldevice ${CURRENT_DIR_SRC})

target_include_directories(logicaldevice PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(logicaldevice PUBLIC physicaldevice)