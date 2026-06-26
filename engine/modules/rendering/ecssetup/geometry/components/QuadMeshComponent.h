#pragma once

// gameplay facing description of a shared quad mesh
// the renderer uses width/height to get a shared quad mesh from MeshManager
// this is the cheap path for generated quads
struct QuadMeshComponent
{
    float width  = 1.0f;
    float height = 1.0f;
};
