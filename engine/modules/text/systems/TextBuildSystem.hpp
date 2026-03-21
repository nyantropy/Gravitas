#pragma once

#include "ECSSimulationSystem.hpp"
#include "QuadTextComponent.h"
#include "TextMeshComponent.h"
#include "GlyphLayoutEngine.h"
#include "TransformComponent.h"
#include "Vertex.h"

// CPU-only simulation system that turns a QuadTextComponent's string into a flat
// quad mesh stored in TextMeshComponent.  No GPU calls are made here.
//
// Runs when QuadTextComponent::dirty == true.  After rebuilding:
//   - QuadTextComponent::dirty    → false
//   - TextMeshComponent::gpuDirty → true  (signals TextBindingSystem to upload)
//
// Layout rules:
//   - Each glyph is a world-space quad in the entity's local XY plane (Z = 0).
//     The entity's TransformComponent then positions/scales the whole block.
//   - X increases rightward, Y increases upward (world-space convention).
//   - '\n' resets the cursor to the left and moves down by font.lineHeight * scale.
//   - Glyphs missing from the atlas are skipped; spaces advance the cursor.
class TextBuildSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float /*dt*/) override
        {
            world.forEach<QuadTextComponent, TransformComponent>(
                [&](Entity e, QuadTextComponent& text, TransformComponent&)
            {
                if (!text.dirty)
                    return;

                text.dirty = false;

                if (!text.font || text.text.empty())
                    return;

                if (!world.hasComponent<TextMeshComponent>(e))
                    world.addComponent(e, TextMeshComponent{});

                auto& tm = world.getComponent<TextMeshComponent>(e);
                tm.vertices.clear();
                tm.indices.clear();

                GlyphLayoutEngine::build(text, tm);

                if (!tm.vertices.empty())
                    tm.gpuDirty = true;
            });
        }
};
