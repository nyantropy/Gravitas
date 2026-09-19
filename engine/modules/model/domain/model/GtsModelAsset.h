#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GlmConfig.h"
#include "GtsModelImage.h"
#include "GtsModelMaterial.h"
#include "GtsModelSkin.h"
#include "GtsVertexAttribute.h"

// 3 types of topology, but we are most likely only gonna use triangles
enum class GtsModelPrimitiveTopology
{
    Points,
    Lines,
    Triangles
};

struct GtsModelPrimitive
{
    // what topo does this primitive have? spoiler its gonna be triangles everytime
    GtsModelPrimitiveTopology topology = GtsModelPrimitiveTopology::Triangles;

    // a primitive can have many attributes, like normals, and positions for example
    // so basically describes what each vertex contains
    std::vector<GtsVertexAttribute> attributes;
    
    // empty indices mean sequential vertices, but if we add indices, 3 of them each will describe one triangle
    std::vector<uint32_t> indices;

    // index for the material this primitive uses
    std::optional<uint32_t> materialIndex;
};

struct GtsModelMesh
{
    std::string name;
    std::vector<GtsModelPrimitive> primitives;
};

struct GtsModelNode
{
    std::string name;
    glm::mat4 localTransform = glm::mat4(1.0f);

    // children are authoritative, meaning that a parent can be derived without duplicated state
    std::vector<uint32_t> children;
    std::optional<uint32_t> meshIndex;
    std::optional<uint32_t> skinBindingIndex;
};

// core struct, everything is accessed with indices
struct GtsModelAsset
{
    std::vector<GtsModelNode> nodes;
    std::vector<uint32_t> rootNodes;
    std::vector<GtsModelMesh> meshes;
    std::vector<GtsModelMaterial> materials;
    std::vector<GtsModelImage> images;
    std::vector<GtsModelSkeletonUse> skeletonUses;
    std::vector<GtsModelSkinBinding> skinBindings;
};
