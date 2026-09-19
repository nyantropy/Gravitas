# Retained UI state and interaction; no renderer, windowing or GPU dependency.
file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(gravitas_ui STATIC ${UI_SOURCES})
target_compile_features(gravitas_ui PUBLIC cxx_std_20)
target_include_directories(gravitas_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gravitas_ui PUBLIC gravitas_core gravitas_rendering_contracts)

if(MSVC)
    target_compile_options(gravitas_ui PRIVATE /W4 /WX- /utf-8)
else()
    target_compile_options(gravitas_ui PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
