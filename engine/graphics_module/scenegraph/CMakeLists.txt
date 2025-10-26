add_library(
    scenegraph

    GtsRenderableObject.hpp
    GtsRenderableObject.cpp

    GtsScene.hpp
    GtsScene.cpp

    GtsSceneNode.hpp
    GtsSceneNode.cpp

    GtsSceneNodeOpt.h
)

#link against external for glm and tinyobj, as well as main library for now, since the modelloader is seperate
target_link_libraries(scenegraph PUBLIC external Gravitas texture)

target_include_directories(scenegraph PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")