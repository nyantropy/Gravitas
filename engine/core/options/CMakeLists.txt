file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(options INTERFACE ${CURRENT_DIR_SRC})

target_include_directories(options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")