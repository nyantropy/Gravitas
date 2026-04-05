#pragma once

// Marker component attached to ghost-block entities (the landing-position shadow of the
// active piece).  Used to:
//   - Skip ghost blocks in the game-over board wipe (they persist across resets)
//   - Let TetrisBlockInitSystem set MaterialComponent::alpha = GHOST_ALPHA
//   - Let TetrisVisualSystem update the texture when the active piece type changes
struct GhostBlockComponent {};
