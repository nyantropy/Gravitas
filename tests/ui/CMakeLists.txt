# Retained UI must remain independently consumable, including renderer-disabled builds.
add_executable(RetainedUiStandaloneTest RetainedUiStandaloneTest.cpp)
target_link_libraries(RetainedUiStandaloneTest PRIVATE gravitas_ui)
add_test(NAME ui_retained_standalone COMMAND RetainedUiStandaloneTest)
set_tests_properties(ui_retained_standalone PROPERTIES LABELS "gravitas;ui;cpu")
