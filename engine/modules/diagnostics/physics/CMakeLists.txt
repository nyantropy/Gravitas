file(GLOB_RECURSE DIAGNOSTICS_PHYSICS_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(gravitas_diagnostics_physics STATIC ${DIAGNOSTICS_PHYSICS_SOURCES})

target_compile_features(gravitas_diagnostics_physics PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_diagnostics_physics PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_diagnostics_physics PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_diagnostics_physics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(gravitas_diagnostics_physics PUBLIC
    gravitas_core
    gravitas_physics
    gravitas_debugdraw
)
