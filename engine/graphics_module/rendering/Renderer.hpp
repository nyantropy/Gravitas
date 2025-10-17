// idea: have an abstract renderer class, so we can implement additional Renderers in the future
// for now we will focus on the simplest rendering technique: forward rendering
#include "RendererConfig.h"

class Renderer 
{
    protected:
        RendererConfig config;
    public:
        explicit Renderer(const RendererConfig& config): config(config) {};
        virtual ~Renderer() = default;

        virtual void createResources() = 0;        
        virtual void renderFrame() = 0;      
};