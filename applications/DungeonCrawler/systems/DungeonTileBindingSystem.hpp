#pragma once

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GraphicsConstants.h"

#include "ProceduralMeshComponent.h"
#include "MaterialComponent.h"
#include "DungeonTileComponent.h"

// Binds DungeonTileComponent entities to ProceduralMeshComponent + MaterialComponent
// so that ProceduralMeshBindingSystem can upload them to the GPU.
//
// Runs once per entity: skips any entity that already has a ProceduralMeshComponent.
// The actual GPU allocation is handled by ProceduralMeshBindingSystem (part of
// installRendererFeature), which runs after this system.
class DungeonTileBindingSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& /*ctx*/) override
    {
        world.forEach<DungeonTileComponent>(
            [&](Entity e, DungeonTileComponent& tile)
        {
            // Only assign once — skip if already bound
            if (world.hasComponent<ProceduralMeshComponent>(e))
                return;

            ProceduralMeshComponent mesh;
            mesh.width  = 1.0f;
            mesh.height = 1.0f;
            world.addComponent(e, mesh);

            MaterialComponent mat;
            mat.texturePath = textureForTile(tile.tileType);
            mat.doubleSided = true;
            world.addComponent(e, mat);
        });
    }

private:
    static std::string textureForTile(TileType type)
    {
        const std::string& RES = GraphicsConstants::ENGINE_RESOURCES;
        switch (type)
        {
            case TileType::StairDown:
            case TileType::StairUp:
                return RES + "/textures/purple_texture.png";

            case TileType::Treasure:
                return RES + "/textures/green_texture.png";

            case TileType::EnemySpawn:
                return RES + "/textures/orange_texture.png";

            default:
                return RES + "/textures/grey_texture.png";
        }
    }
};
