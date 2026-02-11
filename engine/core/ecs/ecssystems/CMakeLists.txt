add_library(ecssystems INTERFACE)

target_link_libraries(ecssystems
    INTERFACE
        abstractsystems
        derivedsystems
)

target_include_directories(ecssystems
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(abstract)
add_subdirectory(derived)