#pragma once
#include <string>

#include "MeshManager.hpp"
#include "Types.h"

class ResourceSystem
{
    private:
        MeshManager meshManager;

    public:
        ResourceSystem() = default;

        // ECS/game logic requests a mesh by path, gets an ID
        mesh_id_type requestMesh(const std::string& path)
        {
            return meshManager.loadMesh(path); // lazy load
        }

        // Renderer resolves mesh ID to actual MeshResource
        MeshResource* getMesh(mesh_id_type id)
        {
            return meshManager.getMesh(id);
        }
};
