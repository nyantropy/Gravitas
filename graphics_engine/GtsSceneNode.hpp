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
            rotationMatrix = glm::rotate(glm::mat4(1.0f), rotationVector.x, glm::vec3(1, 0, 0));
            rotationMatrix = glm::rotate(rotationMatrix, rotationVector.y, glm::vec3(0, 1, 0));
            rotationMatrix = glm::rotate(rotationMatrix, rotationVector.z, glm::vec3(0, 0, 1));
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

        GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim, std::string identifier) : GtsSceneNode(obj, anim)
        {
            this->identifier = identifier;
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

        // Update method
        void update(const glm::mat4& parentTransform, GtsCamera& camera, int framesInFlight, float deltaTime) 
        {
            glm::mat4 modelMatrix = parentTransform * translationMatrix * rotationMatrix * scaleMatrix;
            
            if (renderableObject) 
            {
                glm::mat4 animationMatrix = glm::mat4(1.0f);

                if (animation)
                {
                    animationMatrix = animation->getModelMatrix(deltaTime);
                }

                renderableObject->updateUniforms(modelMatrix * animationMatrix, camera, framesInFlight);
            }

            // Recursively update children
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
