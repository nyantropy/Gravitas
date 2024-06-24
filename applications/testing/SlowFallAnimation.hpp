#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "GtsAnimation.hpp"

class SlowFallAnimation : public GtsAnimation 
{
private:
    float elapsedTime = 0.0f;   // Time accumulator
    float fallInterval = 1.0f;  // Interval in seconds before the object falls by 1.0 unit
    //float currentHeight = 0.0f; // Current height of the object

public:
    void animate(GtsSceneNode* node, float deltaTime) 
    {
        // Accumulate elapsed time
        elapsedTime += deltaTime;
        
        // Check if the accumulated time exceeds the fall interval
        if (elapsedTime >= fallInterval) 
        {
            // Move the object down by 1.0 unit
            //currentHeight -= 1.0f;
            // Reset the elapsed time, subtracting the fall interval to handle the overflow
            elapsedTime -= fallInterval;

            node->translate(glm::vec3(0.0f, -1.0f, 0.0f), "animation");
        }
    }
};