file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(ecsextractors ${CURRENT_DIR_SRC})

target_link_libraries(ecsextractors PUBLIC core gpucomponent)
target_include_directories(ecsextractors PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
