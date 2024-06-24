#pragma once

#include <vector>
#include <string>
#include <cmath>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "GtsRenderableObject.hpp"
#include "GtsAnimation.hpp"
#include "GtsSceneNodeTransformEvent.hpp"

enum LastTransform {translation, rotation, scaling};

class GtsSceneNode 
{
    private:
        std::string identifier;

        glm::vec3 positionVector;
        glm::vec3 rotationVector;
        glm::vec3 scaleVector;

        glm::vec3 lastTransform;
        LastTransform lastTransformType;

        glm::mat4 translationMatrix;
        glm::mat4 rotationMatrix;
        glm::mat4 scaleMatrix;

        std::vector<GtsSceneNode*> children;
        GtsSceneNode* parent;

        GtsRenderableObject* renderableObject;
        GtsAnimation* animation;

        GtsSceneNodeTransformEvent transformEvent;

        bool isActive;

        void updateMatrices() 
        {
            translationMatrix = glm::translate(glm::mat4(1.0f), positionVector);
            rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotationVector.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotationVector.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotationVector.z), glm::vec3(0.0f, 0.0f, 1.0f));
            scaleMatrix = glm::scale(glm::mat4(1.0f), scaleVector);
        }

    public:
        GtsSceneNode() 
        {
            positionVector = glm::vec3(0.0f);
            rotationVector = glm::vec3(0.0f);
            scaleVector = glm::vec3(1.0f);

            translationMatrix = glm::mat4(1.0f);
            rotationMatrix = glm::mat4(1.0f);
            scaleMatrix = glm::mat4(1.0f);
            
            parent = nullptr;
            renderableObject = nullptr;
            animation = nullptr;
            isActive = true;
        }

        GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim) : GtsSceneNode()
        {
            renderableObject = obj;
            animation = anim;
        }

        GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim, std::string identifier) : GtsSceneNode()
        {
            this->identifier = identifier;
            renderableObject = obj;
            animation = anim;
        }

        ~GtsSceneNode()
        {
            if(renderableObject != nullptr)
            {
                delete renderableObject;
            }

            if(animation != nullptr)
            {
                delete animation;
            }

            if(!children.empty())
            {
                for(auto child : children)
                {
                    delete child;
                }
            }
        }

        void subscribeToTransformEvent(std::function<void()> f)
        {
            transformEvent.subscribe(f);
        }

        void setActive(bool active)
        {
            isActive = active;
        }

        bool getActive() const
        {
            return isActive;
        }

        void undoLastTransform()
        {
            switch(lastTransformType)
            {
                case translation:
                    positionVector -= lastTransform;
                    break;
                case rotation:
                    rotationVector -= lastTransform;
                    break;
                //not supported lol im too tired
                case scaling:
                    break;
            }

            updateMatrices();
        }

        void translate(const glm::vec3& offset)
        {
            if (!isActive) return;

            lastTransform = offset;
            lastTransformType = translation;

            positionVector += offset;
            updateMatrices();
            transformEvent.notify();
        }

        void rotate(const glm::vec3& rot)
        {
            if (!isActive) return;

            lastTransform = rot;
            lastTransformType = rotation;

            rotationVector += rot;
            updateMatrices();
            transformEvent.notify();
        }

        void scale(const glm::vec3& scl)
        {
            if (!isActive) return;

            lastTransform = scl;
            lastTransformType = scaling;

            scaleVector *= scl;
            updateMatrices();
            transformEvent.notify();
        }

        void addChild(GtsSceneNode* child)
        {
            child->parent = this;
            children.push_back(child);
        }

        //get the current world position
        glm::vec3 getWorldPosition()
        {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            GtsSceneNode* currentNode = this;

            while (currentNode)
            {
                modelMatrix = currentNode->translationMatrix * currentNode->rotationMatrix * currentNode->scaleMatrix * modelMatrix;
                currentNode = currentNode->parent;
            }

            glm::vec4 worldPosition = modelMatrix * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            return glm::vec3(worldPosition);
        }

        glm::ivec3 worldPositionToGrid(const glm::vec3& worldPos, float gridCellSize)
        {
            int gridX = static_cast<int>(std::round(worldPos.x / gridCellSize));
            int gridY = static_cast<int>(std::round(worldPos.y / gridCellSize));
            int gridZ = static_cast<int>(std::round(worldPos.z / gridCellSize));
            return glm::ivec3(gridX, gridY, gridZ);
        }

        std::vector<glm::ivec3> getGridCoordinates()
        {
            std::vector<glm::ivec3> gridCoordinates;

            for (auto child : children)
            {
                glm::vec3 worldPos = child->getWorldPosition();
                glm::ivec3 gridPos = worldPositionToGrid(worldPos, 1.0f);
                gridCoordinates.push_back(gridPos);
            }

            return gridCoordinates;
        }

        // Update method
        void update(const glm::mat4& parentTransform, GtsCamera& camera, int framesInFlight, float deltaTime) 
        {
            if (!isActive) return;

            if (animation != nullptr)
            {
                animation->animate(this, deltaTime);
            }

            glm::mat4 modelMatrix = parentTransform * translationMatrix * rotationMatrix * scaleMatrix;
            
            if (renderableObject) 
            {
                renderableObject->updateUniforms(modelMatrix, camera, framesInFlight);
            }

            for (auto child : children) 
            {
                child->update(modelMatrix, camera, framesInFlight, deltaTime);
            }
        }

        void draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame)
        {
            if(renderableObject)
            {
                renderableObject->draw(commandBuffer, pipelineLayout, currentFrame);
            }

            for(auto child : children)
            {
                child->draw(commandBuffer, pipelineLayout, currentFrame);
            }
        }

        GtsSceneNode* search(std::string identifier)
        {
            if(this->identifier == identifier)
            {
                return this;    
            }

            for(auto child : children)
            {
                auto res = child->search(identifier);

                if(res != nullptr)
                {
                    return res;
                }
            }

            return nullptr;
        }
};
