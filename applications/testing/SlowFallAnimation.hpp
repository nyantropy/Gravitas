#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "GtsAnimation.hpp"

class SlowFallAnimation : public GtsAnimation 
{
private:
    float elapsedTime = 0.0f;   // Time accumulator
    float fallInterval = 0.5f;  // Interval in seconds before the object falls by 1.0 unit
    float currentHeight = 0.0f; // Current height of the object

public:
    glm::mat4 getModelMatrix(float deltaTime) override {
        // Accumulate elapsed time
        elapsedTime += deltaTime;
        
        // Check if the accumulated time exceeds the fall interval
        if (elapsedTime >= fallInterval) {
            // Move the object down by 1.0 unit
            currentHeight -= 1.0f;
            // Reset the elapsed time, subtracting the fall interval to handle the overflow
            elapsedTime -= fallInterval;
        }
        
        // Create a translation matrix to move the object downwards
        return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, currentHeight, 0.0f));
    }
};