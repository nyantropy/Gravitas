file(GLOB_RECURSE TOOLS_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(gravitas_tools STATIC ${TOOLS_SOURCES})

target_compile_features(gravitas_tools PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(gravitas_tools PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_tools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(gravitas_tools PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/editor"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/inspectors"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "${CMAKE_CURRENT_SOURCE_DIR}/selection"
    "${CMAKE_CURRENT_SOURCE_DIR}/debugdraw"
    "${CMAKE_CURRENT_SOURCE_DIR}/gizmos"
    "${CMAKE_CURRENT_SOURCE_DIR}/cameras"
    "${CMAKE_CURRENT_SOURCE_DIR}/workspace"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/scenes"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_editor"
)

target_link_libraries(gravitas_tools PUBLIC
    gravitas_core
    gravitas_rendering
    gravitas_debugdraw
)
