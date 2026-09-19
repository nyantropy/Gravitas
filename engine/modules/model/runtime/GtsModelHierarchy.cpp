#include "GtsModelHierarchy.h"
#include "model/domain/model/GtsModelAsset.h"
#include <stdexcept>
#include <cmath>

std::vector<glm::mat4> gtsModelNodeTransforms(const GtsModelResource& model)
{
    const auto             nodes = model.nodes();
    std::vector<glm::mat4> transforms(nodes.size(), glm::mat4(1));
    std::vector<bool>      visited(nodes.size());
    struct Pending
    {
        uint32_t  node;
        glm::mat4 parent;
    };
    std::vector<Pending> pending;
    for (auto root : model.rootNodes())
        pending.push_back({root, glm::mat4(1)});
    while (!pending.empty())
    {
        const auto item = pending.back();
        pending.pop_back();
        if (item.node >= nodes.size() || visited[item.node])
            throw std::runtime_error("Invalid model hierarchy at node " + std::to_string(item.node));
        visited[item.node]    = true;
        transforms[item.node] = item.parent * nodes[item.node].localTransform;
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(transforms[item.node][column][row]))
                    throw std::runtime_error("Non-finite model transform at node " + std::to_string(item.node));
        for (auto child : nodes[item.node].children)
            pending.push_back({child, transforms[item.node]});
    }
    for (uint32_t i = 0; i < nodes.size(); ++i)
        if (!visited[i])
            throw std::runtime_error("Unreachable model node " + std::to_string(i));
    return transforms;
}
