file(GLOB_RECURSE PHYSICS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.cpp"
)

add_library(gravitas_physics STATIC ${PHYSICS_SOURCES})
target_compile_features(gravitas_physics PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_physics PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_physics PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_physics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
    "${GTS_ENGINE_ROOT}/engine/modules/transform/components"
    "${GTS_ENGINE_ROOT}/engine/modules/transform/hierarchy"
    "${GTS_ENGINE_ROOT}/engine/modules/transform/runtime"
    "${GTS_ENGINE_ROOT}/engine/modules/transform/scene"
    "${GTS_ENGINE_ROOT}/engine/modules/transform/systems"
)

target_link_libraries(gravitas_physics PUBLIC
    gravitas_core
)
