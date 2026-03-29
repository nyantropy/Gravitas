#pragma once

// Attach to a camera entity to schedule GPU resource release on the next
// CameraResourceClearSystem tick.
//
// destroyAfterClear = true  → entity is destroyed after resources are freed
// destroyAfterClear = false → only this component is removed; entity survives
//
// Usage (typical scene reload):
//   CameraResourceClearComponent c;
//   c.destroyAfterClear = false; // entity destroyed by ecsWorld.clear() below
//   ecsWorld.addComponent(cameraEntity, c);
struct CameraResourceClearComponent
{
    bool destroyAfterClear = true;
};
