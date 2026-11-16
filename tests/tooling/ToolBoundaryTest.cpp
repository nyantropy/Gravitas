#include "ToolBoundaryTest.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include "ECSWorld.hpp"
#include "EngineToolQueries.h"
#include "ToolCameraHandoff.h"
#include "ToolEntityLabelComponent.h"
#include "EngineToolStateComponent.h"
#include "EngineToolCameraSystem.hpp"
#include "EngineGizmoSystem.hpp"
#include "EngineToolWorldPickerSystem.hpp"
#include "TransformWorldResolver.h"

namespace tooling_test
{
    int checks = 0;

    void require(const bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    using tooling_test::require;
    using namespace gts::tools;
    ECSWorld world;
    EcsControllerContext ctx{world};
    EngineToolCameraSystem cameraSystem;
    EngineGizmoSystem gizmoSystem;
    require(!engineToolsVisibility(world).has_value(), "Missing tool state must remain distinguishable");
    require(!isToolCameraActive(world), "Missing camera service must be inactive");
    replaceToolCameraReturnTarget(world, INVALID_ENTITY, INVALID_ENTITY);
    cameraSystem.update(ctx);
    gizmoSystem.update(ctx);
    require(!world.hasAny<EngineToolStateComponent>(), "Observation must not install tools");
    auto& tools = world.createSingleton<EngineToolStateComponent>();
    require(engineToolsVisibility(world) == false, "Installed hidden toolkit must report false");
    const Entity playerCamera = world.createEntity();
    world.addComponent(playerCamera, TransformComponent{{0.0f, 0.0f, 5.0f}});
    world.addComponent(playerCamera, CameraDescriptionComponent{});
    world.getComponent<CameraDescriptionComponent>(playerCamera).active = true;
    const Entity debugCamera = world.createEntity();
    world.addComponent(debugCamera, TransformComponent{{1.0f, 0.0f, 5.0f}});
    world.addComponent(debugCamera, CameraDescriptionComponent{});
    world.getComponent<CameraDescriptionComponent>(debugCamera).active = true;
    world.getComponent<CameraDescriptionComponent>(playerCamera).active = false;
    tools.visible = true;
    const auto copiedVisibility = engineToolsVisibility(world);
    cameraSystem.update(ctx);
    require(copiedVisibility == true && isToolCameraActive(world), "Visible tools must acquire the camera");
    require(world.getSingleton<EngineToolCameraStateComponent>().previousActiveCamera == debugCamera,
            "Tool camera must remember the active debug camera");
    replaceToolCameraReturnTarget(world, playerCamera, playerCamera);
    require(world.getSingleton<EngineToolCameraStateComponent>().previousActiveCamera == debugCamera,
            "Unrelated retiring camera must not replace the return target");
    replaceToolCameraReturnTarget(world, debugCamera, playerCamera);
    require(world.getSingleton<EngineToolCameraStateComponent>().previousActiveCamera == playerCamera,
            "Retiring debug camera must hand back to player camera");
    world.getComponent<CameraDescriptionComponent>(debugCamera).active = false;
    tools.visible = false;
    cameraSystem.update(ctx);
    require(!isToolCameraActive(world) && world.getComponent<CameraDescriptionComponent>(playerCamera).active,
            "Hiding toolkit must restore the handed-off camera");
    require(copiedVisibility == true && engineToolsVisibility(world) == false,
            "Copied observations must not retain mutable state");

    tools.visible = true;
    const Entity parent = world.createEntity();
    world.addComponent(parent, TransformComponent{{2.0f, 0.0f, 0.0f}});
    const Entity selected = world.createEntity();
    world.addComponent(selected, TransformComponent{{1.0f, 0.0f, 0.0f}});
    world.addComponent(selected, ToolEntityLabelComponent{"Selection fixture", "Test", true});
    require(gts::transform::attachToParent(world, selected, parent), "Parent authoring must succeed");
    gts::transform::TransformWorldResolver resolver;
    resolver.resolve(world);
    world.addComponent(selected, BoundsComponent{glm::vec3{-1.0f}, glm::vec3{1.0f}});
    tools.selectedEntity = INVALID_ENTITY;
    auto& view = world.getSingleton<ActiveCameraViewStateComponent>();
    view.valid = true;
    view.viewProjMatrix = glm::orthoRH_ZO(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f)
        * glm::lookAtRH(glm::vec3{3.0f, 0.0f, 5.0f}, glm::vec3{3.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    auto& capture = world.createSingleton<EngineToolInputCaptureComponent>();
    capture.pointerOverViewport = true;
    capture.viewportPointerX = 0.5f;
    capture.viewportPointerY = 0.5f;
    capture.primaryPressed = capture.primaryDown = true;
    EngineToolWorldPickerSystem picker;
    picker.update(ctx);
    require(tools.selectedEntity == selected && tools.selectionSource == EngineToolSelectionSource::WorldPick,
            "Viewport picking must select labeled scene entities");
    capture.worldConsumed = false;
    capture.viewportPointerX = 0.85f;
    gizmoSystem.update(ctx);
    auto& gizmo = world.getSingleton<EngineGizmoStateComponent>();
    require(gizmo.activeAxis == EngineGizmoAxis::X && gizmo.activeEntity == selected,
            "Viewport pointer must select and begin dragging the X handle");
    capture.primaryPressed = false;
    capture.viewportPointerX = 0.95f;
    gizmoSystem.update(ctx);
    require(capture.worldConsumed, "Gizmo drag must consume world input");
    require(std::abs(world.getComponent<TransformComponent>(selected).position.x - 1.2f) < 0.001f,
            "Gizmo drag must convert world movement into parent-local authoring");
    const auto resolved = resolver.resolve(world);
    require(resolved.updatedWorldTransforms > 0
            && std::abs(gts::transform::resolvedWorldMatrix(world, selected)[3].x - 3.2f) < 0.001f,
            "Gizmo must publish through authoritative transform invalidation/resolution");
    capture.primaryReleased = true;
    capture.primaryDown = false;
    gizmoSystem.update(ctx);
    require(gizmo.activeAxis == EngineGizmoAxis::None, "Pointer release must end drag");
    capture.primaryReleased = false;
    capture.primaryDown = true;
    capture.primaryPressed = true;
    gizmoSystem.update(ctx);
    tools.visible = false;
    gizmoSystem.update(ctx);
    require(gizmo.activeAxis == EngineGizmoAxis::None, "Hiding toolkit must cancel drag");
    tools.visible = true;
    tools.selectedEntity = INVALID_ENTITY;
    gizmoSystem.update(ctx);
    require(gizmo.activeAxis == EngineGizmoAxis::None, "Lost selection must be safe");
    world.clear();
    require(!engineToolsVisibility(world) && !isToolCameraActive(world), "Scene reset must release observation state");
    cameraSystem.update(ctx);
    gizmoSystem.update(ctx);
    require(!world.hasAny<EngineToolStateComponent>(), "Scene reset must not resurrect tool control state");
    std::cout << "Tool contracts/camera/gizmo: " << tooling_test::checks << " checks passed\n";
}
