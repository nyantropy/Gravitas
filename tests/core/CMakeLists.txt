add_executable(GtsJsonValueTest GtsJsonValueTest.cpp)
target_link_libraries(GtsJsonValueTest PRIVATE gravitas_core)
add_test(NAME gts_json_value COMMAND GtsJsonValueTest)

add_executable(GtsJsonParserTest GtsJsonParserTest.cpp)
target_link_libraries(GtsJsonParserTest PRIVATE gravitas_core)
add_test(NAME gts_json_parser COMMAND GtsJsonParserTest
    "${GTS_ENGINE_ROOT}/resources" "${GTS_ENGINE_ROOT}/docs"
    "${GTS_ENGINE_ROOT}/benchmarks/fixtures")

add_executable(InputBindingRoutingTest ../input/InputBindingRoutingTest.cpp)
target_link_libraries(InputBindingRoutingTest PRIVATE gravitas_core)
add_test(NAME input_binding_routing COMMAND InputBindingRoutingTest)

add_executable(CoreBoundaryTest CoreBoundaryTest.cpp)
target_link_libraries(CoreBoundaryTest PRIVATE gravitas_core)
add_test(NAME core_boundary COMMAND CoreBoundaryTest)

add_executable(ControllerContextTest ControllerContextTest.cpp)
target_link_libraries(ControllerContextTest PRIVATE gravitas_core)
add_test(NAME controller_context COMMAND ControllerContextTest)
