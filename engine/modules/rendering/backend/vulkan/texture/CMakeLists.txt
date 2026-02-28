file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(vulkantexture ${CURRENT_DIR_SRC})

target_include_directories(vulkantexture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkantexture PUBLIC graphics physicaldevice logicaldevice external graphicsutil)
