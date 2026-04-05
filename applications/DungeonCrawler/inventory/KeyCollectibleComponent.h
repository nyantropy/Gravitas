#pragma once

#include "GlmConfig.h"

#include "Item.h"

struct KeyCollectibleComponent
{
    Item  item;
    int   floorIndex = 0;
    glm::ivec2 gridPosition = {0, 0};
    float rotationSpeed = 1.8f;
};
