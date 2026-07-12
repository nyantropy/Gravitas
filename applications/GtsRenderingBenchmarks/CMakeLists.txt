cmake_minimum_required(VERSION 3.10)
project(GtsRenderingBenchmarks LANGUAGES CXX)

add_executable(GtsRenderingBenchmarks
    main.cpp
)

target_compile_features(GtsRenderingBenchmarks PRIVATE cxx_std_20)

target_link_libraries(GtsRenderingBenchmarks PRIVATE GravitasEngine)

set_target_properties(GtsRenderingBenchmarks PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${GTS_ENGINE_ROOT}"
)

if (WIN32)
    set_target_properties(GtsRenderingBenchmarks PROPERTIES OUTPUT_NAME "GtsRenderingBenchmarks.exe")
endif()
