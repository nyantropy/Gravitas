file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(scriptcomponent ${CURRENT_DIR_SRC})

target_include_directories(scriptcomponent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(scriptcomponent PUBLIC graphics)