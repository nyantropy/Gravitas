#pragma once

#include <glm.hpp>

#include "Types.h"

// Renderer-maintained backend component.  Written by the camera pipeline and
// read exclusively by CameraBindingSystem and RenderCommandExtractor.
//
// Two paths produce matrices into this component each frame:
//
//   Default path (CameraGpuSystem — simulation):
//     Processes entities with CameraDescriptionComponent but WITHOUT
//     CameraOverrideComponent.  Computes view/proj from description + transform,
//     syncs the active flag, and sets dirty = true.
//
//   Override path (custom camera systems, e.g. TetrisCameraSystem — simulation):
//     Processes entities with CameraOverrideComponent.  Writes view/proj directly
//     (e.g. orthographic for 2D), sets active and dirty = true.
//     CameraGpuSystem never touches these entities.
//
//   CameraBindingSystem (controller):
//     Allocates viewID on first encounter, uploads matrices to the camera UBO
//     when dirty == true, then resets dirty = false.
//
// Gameplay systems must not write to this component directly.
struct CameraGpuComponent
{
    view_id_type viewID     = 0;        // 0 = not yet allocated by CameraBindingSystem
    glm::mat4    viewMatrix = glm::mat4(1.0f);
    glm::mat4    projMatrix = glm::mat4(1.0f);

    bool dirty  = true;
    bool active = false;   // true = RenderCommandExtractor should use this camera
};
