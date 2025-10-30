#pragma once
// idea: have an abstract renderer class, so we can implement additional Renderers in the future
// for now we will focus on the simplest rendering technique: forward rendering
#include "RendererConfig.h"
#include "RenderCommand.h"
#include "ECSWorld.hpp"

class Renderer 
{
    protected:
        RendererConfig config;
        explicit Renderer(const RendererConfig& config): config(config) {};
        virtual void createResources() = 0;
    public:
        virtual ~Renderer() = default;        
        virtual void renderFrame(float dt, const std::vector<RenderCommand>& renderList, ECSWorld& ecsWorld) = 0;      
};