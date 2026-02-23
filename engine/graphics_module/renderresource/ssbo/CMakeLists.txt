file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(ssbo ${CURRENT_DIR_SRC})

target_include_directories(ssbo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ssbo PUBLIC graphics bufferutil descriptorset uniformbuffer options)
