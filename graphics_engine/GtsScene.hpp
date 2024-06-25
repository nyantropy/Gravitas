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

    void removeNode(GtsSceneNode* node)
    {
        auto it = std::find(nodes.begin(), nodes.end(), node);
        if (it != nodes.end()) 
        {
            std::cout << "Removed a Node" << std::endl;
            delete *it;
            nodes.erase(it);
        }
    }

    void splitUpNode(GtsSceneNode* node)
    {
        for(auto child : node->getChildren())
        {
            child->assimilateParentTransform();
            nodes.push_back(child);
        }

        if(!node->hasRenderableObject())
        {
            removeNode(node);
        }

        std::cout << "Root nodes:" << countRootNodes() << std::endl;
    }

    void addNodeToParent(GtsSceneNode* node, std::string parentIdentifier)
    {
        GtsSceneNode* result = search(parentIdentifier);
        
        if(result != nullptr)
        {
            result->addChild(node);
        }
        else
        {
            std::cout << "NULLPTR" << std::endl;
        }
    }

    GtsSceneNode* search(std::string identifier)
    {
        for (auto node : nodes)
        {
            auto res = node->search(identifier);

            if(res != nullptr)
            {
                return res;
            }
        }

        return nullptr;
    }

    void update(GtsCamera& camera, int framesInFlight, float deltaTime) 
    {
        for (auto node : nodes) 
        {
            if(node != nullptr)
            {
                node->update(glm::mat4(1.0f), camera, framesInFlight, deltaTime);
            }
        }
    }

    void render(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame) 
    {
        for (auto node : nodes) 
        {
            if(node != nullptr)
            {
                node->draw(commandBuffer, pipelineLayout, currentFrame);  
            }       
        }
    }

    bool empty()
    {
        return nodes.empty();
    }

    int countRootNodes()
    {
        return nodes.size();
    }
};
