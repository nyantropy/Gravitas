#pragma once

#include "struct/TetrominoType.hpp"
#include "struct/TetrominoShape.hpp"
#include "components/TetrisBlockComponent.hpp"
#include "components/NextPieceBlockComponent.hpp"
#include "HierarchyHelper.h"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <deque>
#include <vector>
#include <array>
#include <cstdlib>
#include "GlmConfig.h"

// Owns the upcoming-piece queue and the preview ECS entities shown in the sidebar.
struct PieceQueueController
{
    static constexpr int        QUEUE_SIZE         = 4;
    static constexpr glm::ivec2 NEXT_DISPLAY_PIVOT = { 13, 16 };

    explicit PieceQueueController(Entity nextAnchor) : nextAnchor(nextAnchor) {}

    // Seeds the queue with QUEUE_SIZE random pieces and spawns their preview entities.
    void init(ECSWorld& world)
    {
        if (QUEUE_SIZE <= 0) return;
        nextQueue.clear();
        previewBlocks.resize(QUEUE_SIZE);
        for (int i = 0; i < QUEUE_SIZE; ++i)
        {
            TetrominoType t = (TetrominoType)(rand() % 7);
            nextQueue.push_back(t);
            previewBlocks[i] = spawnSlot(world, t, slotPivot(i));
            for (int j = 0; j < 4; ++j)
                setParent(world, previewBlocks[i][j], nextAnchor);
        }
    }

    bool          hasQueue()  const { return QUEUE_SIZE > 0 && !nextQueue.empty(); }
    TetrominoType front()     const { return nextQueue.front(); }
    bool          hasSecond() const { return QUEUE_SIZE >= 2 && nextQueue.size() >= 2; }
    TetrominoType second()    const { return nextQueue[1]; }

    // Pops the front piece (destroys its entities), pushes a new random piece,
    // slides all previews up one slot, and returns the consumed type.
    TetrominoType advance(ECSWorld& world)
    {
        TetrominoType consumed = nextQueue.front();
        nextQueue.pop_front();

        for (int j = 0; j < 4; ++j)
            world.destroyEntity(previewBlocks[0][j]);
        previewBlocks.erase(previewBlocks.begin());

        TetrominoType newT = (TetrominoType)(rand() % 7);
        nextQueue.push_back(newT);
        previewBlocks.push_back(spawnSlot(world, newT, slotPivot(QUEUE_SIZE - 1)));
        for (int j = 0; j < 4; ++j)
            setParent(world, previewBlocks.back()[j], nextAnchor);

        repositionPreviews(world);
        return consumed;
    }

    // Local pivot for preview slot i relative to the next anchor.
    static glm::ivec2 slotPivot(int i) { return { 0, -i * 3 }; }

private:
    std::deque<TetrominoType>          nextQueue;
    std::vector<std::array<Entity, 4>> previewBlocks;
    Entity                             nextAnchor;

    // Creates four ECS entities for a single preview slot at the given pivot.
    std::array<Entity, 4> spawnSlot(ECSWorld& world, TetrominoType type, glm::ivec2 pivot)
    {
        auto& shape = TetrominoShapes[(int)type][0];
        std::array<Entity, 4> slot;
        for (int j = 0; j < 4; ++j)
        {
            glm::ivec2 p = pivot + shape.blocks[j];
            Entity e = world.createEntity();
            world.addComponent(e, TetrisBlockComponent{ p.x, p.y, true, type });
            world.addComponent(e, NextPieceBlockComponent{});
            slot[j] = e;
        }
        return slot;
    }

    // Repositions all preview entities to match the current queue order.
    void repositionPreviews(ECSWorld& world)
    {
        for (int i = 0; i < (int)previewBlocks.size(); ++i)
        {
            TetrominoType t  = nextQueue[i];
            glm::ivec2 pivot = slotPivot(i);
            auto& shape      = TetrominoShapes[(int)t][0];
            for (int j = 0; j < 4; ++j)
            {
                auto& b    = world.getComponent<TetrisBlockComponent>(previewBlocks[i][j]);
                glm::ivec2 p = pivot + shape.blocks[j];
                b.x = p.x;
                b.y = p.y;
            }
        }
    }
};
