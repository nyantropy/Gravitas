file(GLOB_RECURSE DEBUGDRAW_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(gravitas_debugdraw STATIC ${DEBUGDRAW_SOURCES})

target_compile_features(gravitas_debugdraw PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_debugdraw PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_debugdraw PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_debugdraw PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/physics"
    "${CMAKE_CURRENT_SOURCE_DIR}/primitives"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
)

target_link_libraries(gravitas_debugdraw PUBLIC
    gravitas_core
    gravitas_rendering
    gravitas_physics
)
