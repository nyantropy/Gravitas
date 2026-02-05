file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(abstractsystems ${CURRENT_DIR_SRC})

target_link_libraries(abstractsystems PUBLIC core ecsworld scene)
target_include_directories(abstractsystems PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")