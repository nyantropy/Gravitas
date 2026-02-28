#pragma once

#include "Entity.h"

struct HierarchyComponent 
{
    // -1 or some invalid id stands for root
    Entity parent = {-1};
    //std::vector<Entity> children;    
};