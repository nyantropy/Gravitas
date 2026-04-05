#pragma once

#include "Entity.h"

struct ItemPickedUpEvent
{
    Entity player = INVALID_ENTITY;
    Entity item   = INVALID_ENTITY;
};

struct GoldPickedUpEvent
{
    Entity player = INVALID_ENTITY;
    Entity item   = INVALID_ENTITY;
    int    amount = 0;
};

struct HealthPotionPickedUpEvent
{
    Entity player = INVALID_ENTITY;
    Entity potion = INVALID_ENTITY;
};
