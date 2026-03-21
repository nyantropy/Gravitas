#pragma once

#include <vulkan/vulkan.h>
#include <string>

class GtsFrameGraph;

// Base class for a render stage in the frame graph.
// Override declareResources() to register read/write resource accesses and
// data type dependencies.
// Override record() to emit Vulkan commands.
//
// Stages must not cache resource handles across frames — always query via
// the graph in record() in case layouts or handles change.
class GtsRenderStage
{
public:
    explicit GtsRenderStage(std::string name) : stageName(std::move(name)) {}
    virtual ~GtsRenderStage() = default;

    // Called once during GtsFrameGraph::compile().
    // Register resource reads/writes and data type requests here.
    virtual void declareResources(GtsFrameGraph& graph) = 0;

    // Called every frame during GtsFrameGraph::execute().
    // Emit Vulkan draw/dispatch commands here.
    // Do NOT call vkBeginCommandBuffer / vkEndCommandBuffer — the graph owns those.
    virtual void record(VkCommandBuffer cmd, GtsFrameGraph& graph,
                        uint32_t imageIndex, uint32_t currentFrame) = 0;

    const std::string& getName() const { return stageName; }

protected:
    std::string stageName;
};
