file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(mesh ${CURRENT_DIR_SRC})

target_include_directories(mesh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mesh PUBLIC Gravitas context bufferutil)