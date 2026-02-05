file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(GravitasEngine ${CURRENT_DIR_SRC})

target_include_directories(GravitasEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(GravitasEngine PUBLIC time graphics core scene ecsworld components gtscommand)

target_compile_definitions(GravitasEngine INTERFACE
    GRAVITAS_ENGINE_ROOT="${CMAKE_SOURCE_DIR}"
    GRAVITAS_ENGINE_RESOURCES="${CMAKE_SOURCE_DIR}/resources"
)

add_subdirectory(physics)
add_subdirectory(graphics_module)
add_subdirectory(core)