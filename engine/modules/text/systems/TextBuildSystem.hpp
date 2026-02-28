#pragma once

#include "ECSSimulationSystem.hpp"
#include "TextComponent.h"
#include "TextMeshComponent.h"
#include "TransformComponent.h"
#include "Vertex.h"

// CPU-only simulation system that turns a TextComponent's string into a flat
// quad mesh stored in TextMeshComponent.  No GPU calls are made here.
//
// Runs when TextComponent::dirty == true.  After rebuilding:
//   - TextComponent::dirty    → false
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
            world.forEach<TextComponent, TransformComponent>(
                [&](Entity e, TextComponent& text, TransformComponent&)
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

                buildGeometry(text, tm);

                if (!tm.vertices.empty())
                    tm.gpuDirty = true;
            });
        }

    private:
        void buildGeometry(const TextComponent& text, TextMeshComponent& tm)
        {
            const BitmapFont& font  = *text.font;
            const float       scale = text.scale;

            float cursorX = 0.0f;
            float cursorY = 0.0f;

            for (char ch : text.text)
            {
                if (ch == '\n')
                {
                    cursorX  = 0.0f;
                    cursorY -= font.lineHeight * scale;
                    continue;
                }

                auto it = font.glyphs.find(ch);
                if (it == font.glyphs.end())
                {
                    // Space or unknown glyph: advance by half the line height.
                    cursorX += font.lineHeight * 0.5f * scale;
                    continue;
                }

                const GlyphInfo& g = it->second;

                if (ch != ' ')
                {
                    // Quad corners in local space.
                    // bearing.y is the distance from baseline to top of glyph (positive = up).
                    float x0 = cursorX + g.bearing.x * scale;
                    float y0 = cursorY + g.bearing.y * scale;
                    float x1 = x0 + g.size.x * scale;
                    float y1 = y0 - g.size.y * scale;

                    auto base = static_cast<uint32_t>(tm.vertices.size());

                    constexpr glm::vec3 white = {1.0f, 1.0f, 1.0f};

                    tm.vertices.push_back({{x0, y0, 0.0f}, white, {g.uvMin.x, g.uvMin.y}}); // TL
                    tm.vertices.push_back({{x1, y0, 0.0f}, white, {g.uvMax.x, g.uvMin.y}}); // TR
                    tm.vertices.push_back({{x0, y1, 0.0f}, white, {g.uvMin.x, g.uvMax.y}}); // BL
                    tm.vertices.push_back({{x1, y1, 0.0f}, white, {g.uvMax.x, g.uvMax.y}}); // BR

                    // Two CCW triangles (TL→BL→TR, TR→BL→BR).
                    tm.indices.push_back(base + 0);
                    tm.indices.push_back(base + 2);
                    tm.indices.push_back(base + 1);
                    tm.indices.push_back(base + 1);
                    tm.indices.push_back(base + 2);
                    tm.indices.push_back(base + 3);
                }

                cursorX += g.advance * scale;
            }
        }
};
