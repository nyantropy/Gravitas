file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(instance ${CURRENT_DIR_SRC})

target_include_directories(instance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
#target_link_libraries(instance PUBLIC options)