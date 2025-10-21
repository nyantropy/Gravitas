add_library(
    gcomponents

    MaterialComponent.h
    MeshComponent.h
    TransformComponent.h
)

target_include_directories(gcomponents PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gcomponents PUBLIC Gravitas texture)