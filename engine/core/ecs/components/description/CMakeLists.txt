file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(descriptioncomponent ${CURRENT_DIR_SRC})

target_include_directories(descriptioncomponent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(descriptioncomponent PUBLIC graphics)
