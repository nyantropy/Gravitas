#pragma once

// Marker component that suppresses the baseline DefaultCameraControlSystem
// for this camera entity.
//
// This is DISTINCT from CameraOverrideComponent:
//
//   CameraOverrideComponent       — the camera owns its view / projection matrices.
//                                    CameraGpuSystem skips this entity.
//
//   CameraControlOverrideComponent — the camera owns its control input handling.
//                                    DefaultCameraControlSystem skips this entity.
//                                    CameraGpuSystem is NOT affected.
//
// A camera can hold both, one, or neither of these components independently.
// Tetris uses both: a custom control system drives orbit/zoom intent, and a
// custom GPU system computes the telephoto matrices from that intent.
struct CameraControlOverrideComponent {};
