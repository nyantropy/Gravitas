#pragma once

#include <string>

#include "MeshResource.h"

// a mesh component contains the key to the mesh, which is the path, as well as a ptr to its mesh resource
struct MeshComponent 
{
    std::string meshKey;
    MeshResource* meshPtr = nullptr;
};