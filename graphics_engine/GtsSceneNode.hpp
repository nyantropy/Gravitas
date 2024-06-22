#pragma once

#include <vector>
#include <glm.hpp>
#include "GtsRenderableObject.hpp"
#include "GtsAnimation.hpp"

class GtsSceneNode 
{
    private:
        glm::mat4 translationMatrix;
        glm::mat4 rotationMatrix;
        glm::mat4 scaleMatrix;
        std::vector<GtsSceneNode*> children;
        GtsSceneNode* parent;

        GtsRenderableObject* renderableObject;
        GtsAnimation* animation; // Add an animation member variable

    public:
        GtsSceneNode() 
        {
            translationMatrix = glm::mat4(1.0f);
            rotationMatrix = glm::mat4(1.0f);
            scaleMatrix = glm::mat4(1.0f);
            parent = nullptr;
            renderableObject = nullptr;
            animation = nullptr;
        }

        GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim) : GtsSceneNode()
        {
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
        }

        void setRenderableObject(GtsRenderableObject* obj) 
        {
            renderableObject = obj;
        }

        void setAnimation(GtsAnimation* anim)
        {
            animation = anim;
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
};
