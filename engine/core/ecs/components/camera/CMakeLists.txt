file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(cameracomponent ${CURRENT_DIR_SRC})

target_include_directories(cameracomponent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(cameracomponent PUBLIC graphics)