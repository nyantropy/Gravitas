file(GLOB_RECURSE PHYSICS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/*.cpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/core/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/debug"
    "${CMAKE_SOURCE_DIR}/engine/modules/transform/components"
    "${CMAKE_SOURCE_DIR}/engine/modules/hierarchy/components"
)

target_link_libraries(gravitas_physics PUBLIC
    gravitas_core
    gravitas_rendering
)
