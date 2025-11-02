file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(gsystems ${CURRENT_DIR_SRC})

target_include_directories(gsystems PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gsystems PUBLIC components cameracomponent animationcomponent ecs mesh)