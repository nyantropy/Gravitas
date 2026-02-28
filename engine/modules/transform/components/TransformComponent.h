#pragma once

#include <glm.hpp>

// just default transform as standard values
struct TransformComponent 
{
    glm::vec3 position  = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotation  = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 scale     = glm::vec3(1.0f, 1.0f, 1.0f);

    glm::mat4 getModelMatrix() const
    {
        glm::mat4 model = glm::mat4(1.0f);

        // Translate
        model = glm::translate(model, position);

        // Rotate around X, Y, Z (order matters)
        model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

        // Scale
        model = glm::scale(model, scale);

        return model;
    }
};
