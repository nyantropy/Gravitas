file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(scene ${CURRENT_DIR_SRC})

target_link_libraries(scene PUBLIC core components descriptioncomponent animationcomponent time gtscommand derivedsystems options)
target_include_directories(scene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")