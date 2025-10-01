#pragma once

#include <vector>
#include <string>
#include <cmath>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/string_cast.hpp> 

#include "GtsSceneNodeTransformEvent.hpp"

// describes something that is rotatable in the context of the engine
// this whole concept will likely not make it in the long run if we go with an entity based system instead like most newer engines
// but for right now its just here, not doing anything
class Rotatable 
{
    protected:
        glm::vec3 rotationVector;
        bool isRotatable;
    
    private:
        glm::mat4 calculateRotationMatrix() 
        {
            glm::quat quatX = glm::angleAxis(glm::radians(rotationVector.x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat quatY = glm::angleAxis(glm::radians(rotationVector.y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat quatZ = glm::angleAxis(glm::radians(rotationVector.z), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat combinedQuat = quatZ * quatY * quatX;

            return glm::toMat4(combinedQuat);
        }

    public:
        Rotatable() : rotationVector(0.0f), isRotatable(true) {}
        ~Rotatable() {}

        glm::mat4 getRotationMatrix()
        {
            return this->calculateRotationMatrix();
        }

        glm::vec3 getRotationVector()
        {
            return this->rotationVector;
        }

        void rotate(const glm::vec3& rot) 
        {
            if (!this->isRotatable) return;

            //lastTransform = rot;
            //lastTransformType = rotation;

            rotationVector += rot;
            //transformEvent.notify();
        }
};
