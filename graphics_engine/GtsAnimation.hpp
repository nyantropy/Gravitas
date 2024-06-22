#pragma once

#include <glm.hpp>

class GtsAnimation
{
    public:   
        virtual glm::mat4 getModelMatrix(float deltaTime) = 0;
};