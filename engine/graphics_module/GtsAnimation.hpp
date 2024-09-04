#pragma once

#include <glm.hpp>

#include "GtsSceneNode.hpp"

class GtsSceneNode;

class GtsAnimation
{
    public:   
        virtual void animate(GtsSceneNode* node, float deltaTime) = 0;
};