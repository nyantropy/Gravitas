file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(derivedsystems ${CURRENT_DIR_SRC})

target_link_libraries(derivedsystems PUBLIC core ecsworld abstractsystems)
target_include_directories(derivedsystems PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")