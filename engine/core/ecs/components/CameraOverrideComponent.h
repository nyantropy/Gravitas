#pragma once

// Marker component that flags a camera entity as fully custom-controlled.
//
// When present on an entity, CameraGpuSystem will skip it entirely.
// The custom camera system that owns this entity must:
//   - iterate on CameraDescriptionComponent + CameraOverrideComponent
//   - read lens parameters (fov, aspect, near, far) from CameraDescriptionComponent
//   - compute and write viewMatrix / projMatrix into CameraGpuComponent
//   - set CameraGpuComponent::active = desc.active
//   - set CameraGpuComponent::dirty  = true
//
// GPU upload and render-view allocation remain in CameraBindingSystem, which
// iterates on CameraDescriptionComponent and is unaware of this distinction.
struct CameraOverrideComponent {};
