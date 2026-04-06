#pragma once

#include "Entity.h"

struct ItemPickedUpEvent
{
    Entity player{};
    Entity item{};
};

struct GoldPickedUpEvent
{
    Entity player{};
    int    amount = 0;
};

struct HealthPotionPickedUpEvent
{
    Entity player{};
    Entity potion{};
};
