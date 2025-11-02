#pragma once

#include <glm.hpp>

#include "AnimationMode.h"

// a component, that when added to an entity, allows it to be animated
struct AnimationComponent
{
    // a simple trigger to enable/disable animation
    bool enabled = true;

    // determines if this animation will occur in global or local space
    bool localSpace = true;

    // the bitmask of AnimationMode values
    uint32_t activeModes = 0;

    // rotation animation settings, if active
    glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    float rotationSpeed = 0.0f;

    // translation animation settings, if active
    glm::vec3 translationAxis = glm::vec3(0.0f);
    float translationAmplitude = 0.0f;
    float translationSpeed = 0.0f;

    // scaling animation settings, if active
    glm::vec3 scaleAmplitude = glm::vec3(0.0f);
    float scaleSpeed = 0.0f;

    // animation time
    float time = 0.0f;

    // needed if we want to retain initial transform values
    glm::vec3 initialPosition = glm::vec3(0.0f);
    glm::vec3 initialRotation = glm::vec3(0.0f);
    glm::vec3 initialScale    = glm::vec3(1.0f);
    bool initialized = false;

    // ---------------------------
    //Functions to trigger/disable animation modes if needed

    inline bool hasMode(AnimationMode mode) const 
    {
        return (activeModes & static_cast<uint32_t>(mode)) != 0;
    }

    inline void enableMode(AnimationMode mode) 
    {
        activeModes |= static_cast<uint32_t>(mode);
    }

    inline void disableMode(AnimationMode mode) 
    {
        activeModes &= ~static_cast<uint32_t>(mode);
    }
    
    //----------------------------
};