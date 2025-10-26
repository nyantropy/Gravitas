#pragma once

#include <string>

#include "MeshResource.h"
#include "Types.h"

// a mesh component now only contains the id of whichever mesh it currently uses
struct MeshComponent 
{
    mesh_id_type meshID;
};