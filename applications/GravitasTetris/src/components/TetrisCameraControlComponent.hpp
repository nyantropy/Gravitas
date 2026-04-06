#pragma once

// Stores the camera control intent for the Tetris telephoto camera.
// Written by TetrisCameraControlSystem (input → intent).
// Read by TetrisCameraSystem (intent → view/proj matrices).
//
// Separating intent from matrix computation keeps control logic out of
// the GPU system and matrix logic out of the control system.
struct TetrisCameraControlComponent
{
    // Rotation around the world Y axis (radians).  0 = straight-on view.
    float orbitAngle   = 0.0f;

    // Distance from the framed board center to the camera.
    // Matches TetrisCameraSystem's original hard-coded value.
    float zoomDistance = 180.0f;
};
