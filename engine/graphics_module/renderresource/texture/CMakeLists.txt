file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(texture ${CURRENT_DIR_SRC})

target_include_directories(texture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(texture PUBLIC Gravitas physicaldevice logicaldevice external descriptorset)