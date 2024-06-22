#pragma once

#include <vector>

#include "GtsSceneNode.hpp"
#include "GtsCamera.hpp"

class GtsScene 
{
private:
    std::vector<GtsSceneNode*> nodes;

public:
    GtsScene()
    {

    }

    ~GtsScene()
    {
        if(!nodes.empty())
        {
            for (auto node : nodes) 
            {
                delete node;
            }
        }
    }

    void addNode(GtsSceneNode* node) 
    {
        nodes.push_back(node);
    }

    void update(GtsCamera& camera, int framesInFlight, float deltaTime) 
    {
        for (auto node : nodes) 
        {
            node->update(glm::mat4(1.0f), camera, framesInFlight, deltaTime);
        }
    }

    void render(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame) 
    {
        for (auto node : nodes) 
        {
            node->draw(commandBuffer, pipelineLayout, currentFrame);
        }
    }

    bool empty()
    {
        return nodes.empty();
    }
};
