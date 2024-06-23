#pragma once

#include <string>
#include <glm.hpp>

#include "GtsRenderableObject.hpp"
#include "GtsAnimation.hpp"

struct GtsSceneNodeOpt
{
    GtsRenderableObject* objectPtr = nullptr;
    GtsAnimation* animPtr = nullptr;

    std::string identifier;
    std::string parentIdentifier;

    glm::vec3 translationVector = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotationVector = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 scaleVector = glm::vec3(0.0f, 0.0f, 0.0f);
};