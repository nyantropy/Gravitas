file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(textsys ${CURRENT_DIR_SRC})

target_link_libraries(textsys PUBLIC
    core
    ecsworld
    abstractsystems
    gpucomponent
    descriptioncomponent
    components
    renderresource
    mesh
)

target_include_directories(textsys PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
