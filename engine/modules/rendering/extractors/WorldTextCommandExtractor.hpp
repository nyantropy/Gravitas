#pragma once

#include <vector>

#include "GlmConfig.h"

#include "ECSWorld.hpp"
#include "CameraGpuComponent.h"
#include "QuadTextComponent.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"
#include "GlyphLayoutEngine.h"
#include "TextCommand.h"
#include "Vertex.h"

// Extracts world-space text from QuadTextComponent + TransformComponent entities,
// transforms the glyph quads into NDC [0..1] using the active camera's
// view-projection matrix, and returns them as TextCommandLists ready for the
// TextRenderStage.  Hierarchy is handled by walking the parent chain.
//
// Called from GravitasEngine::render() before renderFrame.  The returned lists
// are merged into the screen-space uiLists so both arrive in one batch.
class WorldTextCommandExtractor
{
public:
    std::vector<TextCommandList> extract(ECSWorld& world)
    {
        std::vector<TextCommandList> result;

        // Find the active camera's view-projection matrix.
        glm::mat4 viewProj = glm::mat4(1.0f);
        world.forEach<CameraGpuComponent>([&](Entity, CameraGpuComponent& cam)
        {
            if (cam.active)
                viewProj = cam.projMatrix * cam.viewMatrix;
        });

        // Generate and transform world-space text quads.
        world.forEach<QuadTextComponent, TransformComponent>(
            [&](Entity e, QuadTextComponent& qtc, TransformComponent& tc)
        {
            if (!qtc.font || qtc.text.empty()) return;

            glm::mat4 worldMat = computeWorldMatrix(world, e, tc);
            glm::mat4 mvp      = viewProj * worldMat;

            // Generate local-space vertices via GlyphLayoutEngine::build.
            std::vector<Vertex>   localVerts;
            std::vector<uint32_t> localIndices;
            GlyphLayoutEngine::build(qtc, localVerts, localIndices);

            if (localVerts.empty()) return;

            // Find or create the batch for this atlas.
            TextCommandList* cmd = nullptr;
            for (auto& b : result)
                if (b.textureID == qtc.font->atlasTexture) { cmd = &b; break; }
            if (!cmd)
            {
                result.push_back({});
                cmd = &result.back();
                cmd->textureID = qtc.font->atlasTexture;
            }

            uint32_t baseIdx = static_cast<uint32_t>(cmd->vertices.size());

            // Transform local vertices to NDC [0..1] and append.
            for (const Vertex& v : localVerts)
            {
                glm::vec4 clip = mvp * glm::vec4(v.pos.x, v.pos.y, v.pos.z, 1.0f);
                float ndcX = clip.x / clip.w;
                float ndcY = clip.y / clip.w;
                // Convert from NDC [-1,1] to viewport [0,1].
                cmd->vertices.push_back(
                    {{ndcX * 0.5f + 0.5f, ndcY * 0.5f + 0.5f},
                     {v.texCoord.x, v.texCoord.y},
                     {1.0f, 1.0f, 1.0f, 1.0f}});
            }

            // Remap indices to the concatenated vertex base.
            for (uint32_t idx : localIndices)
                cmd->indices.push_back(baseIdx + idx);
        });

        return result;
    }

private:
    // Compute the world-space model matrix for an entity, walking the parent chain.
    static glm::mat4 computeWorldMatrix(ECSWorld& world, Entity e,
                                         const TransformComponent& tc)
    {
        glm::mat4 local = tc.getModelMatrix();

        if (world.hasComponent<HierarchyComponent>(e))
        {
            const auto& h = world.getComponent<HierarchyComponent>(e);
            if (h.parent != INVALID_ENTITY &&
                world.hasComponent<TransformComponent>(h.parent))
            {
                const auto& parentTc = world.getComponent<TransformComponent>(h.parent);
                glm::mat4 parentWorld = computeWorldMatrix(world, h.parent, parentTc);
                return parentWorld * local;
            }
        }

        return local;
    }
};
