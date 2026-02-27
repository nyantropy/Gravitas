#pragma once

// Marker component that flags a camera entity as fully custom-controlled.
//
// When present on an entity, CameraGpuSystem will skip it entirely, delegating
// all matrix computation to whatever custom camera system owns it.
//
// Custom camera systems (e.g. TetrisCameraSystem) must:
//   - iterate on CameraOverrideComponent + CameraGpuComponent
//   - write viewMatrix, projMatrix, active, and dirty directly into CameraGpuComponent
//   - run before CameraGpuSystem in the simulation pipeline
//
// The GPU upload and render-view allocation remain in CameraBindingSystem,
// which is unaware of this distinction and processes all CameraGpuComponents.
struct CameraOverrideComponent {};
