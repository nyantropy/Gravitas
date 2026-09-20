if(NOT TARGET gravitas_model_frontend OR NOT TARGET gravitas_runtime_execution)
    message(STATUS "GtsPerformanceBenchmarks unavailable: model/transform/policy contracts are required")
    return()
endif()

set(GTS_PERF_FIXTURE_SOURCE "${GTS_ENGINE_ROOT}/tests/assets/fixtures/canonical-cooking/triangle.obj")
configure_file("${GTS_PERF_FIXTURE_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/fixtures/triangle.obj" COPYONLY)
set(GTS_PERF_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GTS_PERF_GENERATED}")
add_custom_target(GtsPerformanceBuildMetadata
    COMMAND "${CMAKE_COMMAND}"
        "-DENGINE_ROOT=${GTS_ENGINE_ROOT}"
        "-DBUILD_ROOT=${CMAKE_BINARY_DIR}"
        "-DOUTPUT=${GTS_PERF_GENERATED}/BuildMetadata.h"
        "-DCONFIG=$<CONFIG>"
        "-DCOMPILER=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        "-DCOMPILER_PATH=${CMAKE_CXX_COMPILER}"
        "-DPLATFORM=${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}"
        "-DFLAGS=${CMAKE_CXX_FLAGS} $<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>$<$<CONFIG:Release>:${CMAKE_CXX_FLAGS_RELEASE}>$<$<CONFIG:RelWithDebInfo>:${CMAKE_CXX_FLAGS_RELWITHDEBINFO}>$<$<CONFIG:MinSizeRel>:${CMAKE_CXX_FLAGS_MINSIZEREL}>"
        "-DFIXTURE=${GTS_PERF_FIXTURE_SOURCE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/harness/BuildMetadata.cmake"
    BYPRODUCTS "${GTS_PERF_GENERATED}/BuildMetadata.h"
    VERBATIM
)
add_executable(GtsPerformanceBenchmarks
    main.cpp
    harness/BenchmarkRunner.cpp
    harness/BenchmarkMetrics.cpp
    harness/BenchmarkReport.cpp
    workloads/TransformScaling.cpp
    workloads/ModelScaling.cpp
)
add_dependencies(GtsPerformanceBenchmarks GtsPerformanceBuildMetadata)
target_compile_features(GtsPerformanceBenchmarks PRIVATE cxx_std_20)
target_include_directories(GtsPerformanceBenchmarks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/harness"
    "${CMAKE_CURRENT_SOURCE_DIR}/workloads"
    "${GTS_PERF_GENERATED}"
)
target_compile_definitions(GtsPerformanceBenchmarks PRIVATE
    GTS_PERF_MODEL_FIXTURE="${CMAKE_CURRENT_BINARY_DIR}/fixtures/triangle.obj")
target_link_libraries(GtsPerformanceBenchmarks PRIVATE
    gravitas_core gravitas_transform gravitas_transform_animation
    gravitas_model_loading gravitas_model_realization gravitas_model_frontend
    gravitas_material_frontend
    gravitas_runtime_execution
)
