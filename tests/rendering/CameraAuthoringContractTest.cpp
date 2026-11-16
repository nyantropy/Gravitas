#include "CameraAuthoringContractTest.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "ECSWorld.hpp"
#include "SceneExecutionPolicy.h"
#include "RendererCameraSceneFeature.h"
#include "TransformSceneFeature.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "ActiveCameraViewStateComponent.h"
#include "CameraControlOverrideComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraOverrideComponent.h"
#include "RenderableRuntimeStatus.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntimeAccess.h"
#include "MaterialRuntimeRegistry.h"

namespace
{
    unsigned assertions = 0;
    void     require(const bool condition, const char* message)
    {
        ++assertions;
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

view_id_type CameraContractResources::requestCameraBuffer()
{
    return ++allocations;
}

void CameraContractResources::releaseCameraBuffer(view_id_type)
{
    ++releases;
}

void CameraContractResources::uploadCameraView(view_id_type, const glm::mat4& view, const glm::mat4& projection)
{
    ++uploads;
    lastView       = view;
    lastProjection = projection;
}

void requireCameraMatrixNear(const glm::mat4& actual, const glm::mat4& expected)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
        {
            if (std::abs(actual[column][row] - expected[column][row]) >= 0.0001f)
                std::cerr << "Camera mismatch at check " << assertions << " [" << column << "][" << row
                          << "]: actual=" << actual[column][row] << " expected=" << expected[column][row] << '\n';
            require(std::abs(actual[column][row] - expected[column][row]) < 0.0001f,
                    "Camera matrix differs from authored pose/lens");
        }
}

int main()
try
{
    CameraContractResources    resources;
    ECSWorld                   world;
    const Entity               first  = world.createEntity();
    const Entity               second = world.createEntity();
    CameraDescriptionComponent firstDescription;
    firstDescription.active   = true;
    firstDescription.target   = {1, 2, -4};
    firstDescription.fov      = glm::radians(73.0f);
    firstDescription.nearClip = 0.07f;
    firstDescription.farClip  = 710.0f;
    TransformComponent firstPose;
    firstPose.position = {1, 2, 3};
    world.addComponent(first, firstDescription);
    world.addComponent(first, firstPose);
    world.addComponent(first, CameraControlOverrideComponent{});
    world.addComponent(second, CameraDescriptionComponent{});
    world.addComponent(second, TransformComponent{});
    world.addComponent(second, CameraControlOverrideComponent{});
    const auto execution = gts::execution::rendererExecutionInputs();
    gts::transform::installTransformFeature(world, execution.defaultSelection, execution.preparation);
    gts::rendering::installRendererCameraSceneFeature(world, &resources, execution);
    EcsControllerContext context{world};
    auto&                rendering = gts::rendering::controllerContext(context);
    rendering.resources            = &resources;
    TimeContext time;
    context.time = &time;
    for (const float aspect : {4.0f / 3.0f, 16.0f / 9.0f, 0.75f})
    {
        world.getComponent<CameraDescriptionComponent>(first).aspectRatio = aspect;
        world.updateControllers(context);
        const auto& description = world.getComponent<CameraDescriptionComponent>(first);
        const auto& view        = world.getSingleton<ActiveCameraViewStateComponent>();
        require(view.valid && view.activeCamera == first, "Descriptor activation did not reach active view");
        requireCameraMatrixNear(view.viewMatrix, glm::lookAt(firstPose.position, description.target, description.up));
        auto expected = glm::perspective(description.fov, aspect, description.nearClip, description.farClip);
        expected[1][1] *= -1.0f;
        requireCameraMatrixNear(view.projMatrix, expected);
        require(resources.allocations == 2 && resources.uploads == 0,
                "CPU lifecycle reallocated camera buffers or bypassed the renderer upload fence");
    }
    world.getComponent<CameraDescriptionComponent>(first).active  = false;
    world.getComponent<CameraDescriptionComponent>(second).active = true;
    world.getComponent<CameraDescriptionComponent>(second).target = {0, 0, -1};
    world.updateControllers(context);
    require(world.getSingleton<ActiveCameraViewStateComponent>().activeCamera == second,
            "Descriptor handoff did not deactivate old GPU camera");
    world.getComponent<TransformComponent>(first).position = {5, 3, 4};
    gts::transform::markDirty(world, first);
    world.getComponent<CameraDescriptionComponent>(first).active  = true;
    world.getComponent<CameraDescriptionComponent>(second).active = false;
    world.updateControllers(context);
    requireCameraMatrixNear(world.getSingleton<ActiveCameraViewStateComponent>().viewMatrix,
                            glm::lookAt(glm::vec3(5, 3, 4), firstDescription.target, firstDescription.up));
    world.removeComponent<CameraDescriptionComponent>(second);
    world.updateControllers(context);
    require(resources.releases == 1, "Camera descriptor removal did not release its buffer");
    world.clear();
    require(resources.releases == 2, "World clear did not release remaining camera buffer");
    const auto   runtimeCount = gts::rendering::materialRuntimeRegistry().size();
    const Entity observed     = world.createEntity();
    world.addComponent(observed, MaterialReferenceComponent{});
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        const auto status = gts::rendering::renderableRuntimeStatus(world, observed);
        require(!status.meshGpuReady && !status.materialGpuReady && !status.renderReady,
                "Unrealized descriptor reported renderer readiness");
        require(gts::rendering::findMaterialRuntime(world) == nullptr &&
                    gts::rendering::materialRuntimeRegistry().size() == runtimeCount,
                "Readiness observation created material/world runtime state");
    }
    auto&      materials  = gts::rendering::materialRuntime(world);
    const auto queueCount = gts::rendering::MaterialRuntimeAccess::queuedMaterialSyncCount(materials);
    gts::rendering::renderableRuntimeStatus(world, observed);
    require(gts::rendering::MaterialRuntimeAccess::queuedMaterialSyncCount(materials) == queueCount &&
                gts::rendering::MaterialRuntimeAccess::gpuStateCount(materials) == 0,
            "Readiness observation changed synchronization work or allocated GPU state");
    std::cout << "Camera authoring contract passed: " << assertions << " checks\n";
    return 0;
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
