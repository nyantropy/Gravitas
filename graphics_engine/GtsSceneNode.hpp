#pragma once

#include <vector>
#include <string>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "GtsRenderableObject.hpp"
#include "GtsAnimation.hpp"

class GtsSceneNode 
{
    private:
        std::string identifier;

        glm::vec3 positionVector;
        glm::vec3 rotationVector;
        glm::vec3 scaleVector;

        glm::mat4 translationMatrix;
        glm::mat4 rotationMatrix;
        glm::mat4 scaleMatrix;

        std::vector<GtsSceneNode*> children;
        GtsSceneNode* parent;

        GtsRenderableObject* renderableObject;
        GtsAnimation* animation;

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

            updateMatrices();
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

        void setRenderableObject(GtsRenderableObject* obj) 
        {
            renderableObject = obj;
        }

        void setAnimation(GtsAnimation* anim)
        {
            animation = anim;
        }

        void setPosition(const glm::vec3& pos)
        {
            positionVector = pos;
            updateMatrices();
        }

        void setRotation(const glm::vec3& rot)
        {
            rotationVector = rot;
            updateMatrices();
        }

        void setScale(const glm::vec3& scl)
        {
            scaleVector = scl;
            updateMatrices();
        }

        void translate(const glm::vec3& offset)
        {
            positionVector += offset;
            updateMatrices();
        }

        void rotate(const glm::vec3& rot)
        {
            rotationVector += rot;
            updateMatrices();
        }

        void scale(const glm::vec3& scl)
        {
            scaleVector *= scl;
            updateMatrices();
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
                modelMatrix = currentNode->translationMatrix * modelMatrix;
                currentNode = currentNode->parent;
            }

            glm::vec4 worldPosition = modelMatrix * glm::vec4(positionVector, 1.0f);
            return glm::vec3(worldPosition);
        }

        glm::ivec3 worldPositionToGrid(const glm::vec3& worldPos, float gridCellSize)
        {
            int gridX = static_cast<int>(worldPos.x / gridCellSize);
            int gridY = static_cast<int>(worldPos.y / gridCellSize);
            int gridZ = static_cast<int>(worldPos.z / gridCellSize);
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
            glm::mat4 modelMatrix = parentTransform * translationMatrix * rotationMatrix * scaleMatrix;

            glm::mat4 animationMatrix = glm::mat4(1.0f);

            if (animation != nullptr)
            {
                animationMatrix = animation->getModelMatrix(deltaTime);
            }

            //animation needs to go from the left, not the right :)
            glm::mat4 finalMatrix = animationMatrix * modelMatrix;
            
            if (renderableObject) 
            {
                renderableObject->updateUniforms(finalMatrix, camera, framesInFlight);
            }

            for (auto child : children) 
            {
                child->update(finalMatrix, camera, framesInFlight, deltaTime);
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
