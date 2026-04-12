#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Entity.h"
#include "GlmConfig.h"
#include "Types.h"

struct AABB
{
    glm::vec3 min{0.0f, 0.0f, 0.0f};
    glm::vec3 max{0.0f, 0.0f, 0.0f};
};

using FrustumPlanes = std::array<glm::vec4, 6>;
using RenderableID = entity_id_type;

struct RenderableSnapshot
{
    RenderableID  id = 0;
    Entity        entity{0};
    glm::mat4     modelMatrix = glm::mat4(1.0f);

    AABB          worldBounds{};
    bool          hasBounds = false;

    mesh_id_type    meshID = 0;
    texture_id_type textureID = 0;
    ssbo_id_type    objectSSBOSlot = 0;

    float         alpha = 1.0f;
    bool          doubleSided = false;
    bool          visible = true;
    uint64_t      sortKey = 0;
};

struct RenderExtractionSnapshot
{
    std::vector<RenderableSnapshot> renderables;
    FrustumPlanes                   frustum{};
    view_id_type                    cameraViewID = 0;
};
