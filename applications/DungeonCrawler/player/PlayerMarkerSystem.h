#pragma once

#include "Entity.h"

class ECSWorld;

class PlayerMarkerSystem
{
public:
    Entity spawnMarker(ECSWorld& world) const;
    void syncMarker(ECSWorld& world, Entity playerEntity, Entity markerEntity) const;
};
