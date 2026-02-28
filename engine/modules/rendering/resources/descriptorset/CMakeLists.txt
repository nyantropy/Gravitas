file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(descriptorset ${CURRENT_DIR_SRC})

target_include_directories(descriptorset PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(descriptorset PUBLIC graphics texture)