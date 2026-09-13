#include "SkinnedFrameExtraction.h"
#include <stdexcept>

void require(bool condition)
{
    if (!condition) throw std::runtime_error("Skinned frame ownership invariant failed");
}
int main()
{
    ECSWorld world;
    auto data = std::make_shared<GtsSkinnedModelData>();
    data->bindingCount = 2;
    auto instance = std::make_shared<GtsSkinnedModelInstance>(GtsSkinnedModelInstance{data});
    auto palettes = std::make_shared<std::vector<GtsSkinPalette>>(2);
    (*palettes)[0].matrices.push_back(glm::mat4(1));
    (*palettes)[1].matrices.push_back(glm::mat4(2));
    SkinnedModelComponent mesh{instance, palettes, glm::mat4(1)};
    mesh.actorFromReference[3].y = -1.02f;
    auto entity = world.createEntity();
    world.addComponent(entity, mesh);
    WorldTransformComponent placement;
    placement.matrix[3] = {10, 1.02f, 3, 1};
    world.addComponent(entity, placement);
    auto snapshot = extractSkinnedFrame(world, 7);
    require(snapshot.cameraViewID == 7 && snapshot.draws.size() == 1);
    require(snapshot.draws[0].worldFromReference[3] == glm::vec4(10, 0, 3, 1));
    require(snapshot.draws[0].palettes == palettes);
    require(snapshot.draws[0].palettes->at(1).matrices[0] == glm::mat4(2));
    auto replacement = std::make_shared<std::vector<GtsSkinPalette>>(*palettes);
    replacement->at(0).matrices[0][3].x = 4;
    world.getComponent<SkinnedModelComponent>(entity).palettes = replacement;
    auto updated = extractSkinnedFrame(world, 7);
    require(updated.draws[0].instance == snapshot.draws[0].instance);
    require(updated.draws[0].worldFromReference == snapshot.draws[0].worldFromReference);
    require(snapshot.draws[0].palettes->at(0).matrices[0][3].x == 0);
    require(updated.draws[0].palettes->at(0).matrices[0][3].x == 4);
}
