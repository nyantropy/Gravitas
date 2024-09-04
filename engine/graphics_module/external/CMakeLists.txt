add_library(
    external
    stb_image_impl.cpp
    tiny_obj_loader_impl.cpp
)

target_include_directories(external PUBLIC
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/glm/glm
)