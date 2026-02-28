file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(camera ${CURRENT_DIR_SRC})

target_include_directories(camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(camera PUBLIC graphics bufferutil descriptorset options)
