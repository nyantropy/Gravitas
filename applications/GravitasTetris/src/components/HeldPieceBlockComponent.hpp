#pragma once

// Marker component attached to the four held-piece display entities.
// Used to:
//   - Skip held display blocks in the game-over board wipe (they persist across resets)
//   - Let TetrisVisualSystem rebind the texture when the held piece type changes on swap
struct HeldPieceBlockComponent {};
