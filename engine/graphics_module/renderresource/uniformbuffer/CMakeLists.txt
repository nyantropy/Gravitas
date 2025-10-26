file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(uniformbuffer ${CURRENT_DIR_SRC})

target_include_directories(uniformbuffer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(uniformbuffer PUBLIC Gravitas bufferutil ecs options)