add_library(gravitas_debugdraw STATIC systems/DebugDrawSystem.cpp)

target_compile_features(gravitas_debugdraw PUBLIC cxx_std_20)

target_include_directories(gravitas_debugdraw PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/primitives"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
)

target_link_libraries(gravitas_debugdraw PUBLIC
    gravitas_core
    gravitas_transform
    gravitas_rendering
)

target_link_libraries(gravitas_debugdraw PUBLIC gravitas_execution_policy)
