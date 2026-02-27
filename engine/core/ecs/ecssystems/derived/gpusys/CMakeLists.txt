file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(gpusys ${CURRENT_DIR_SRC})

target_link_libraries(gpusys PUBLIC core ecsworld abstractsystems gpucomponent descriptioncomponent)
target_include_directories(gpusys PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
