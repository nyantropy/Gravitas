#pragma once

// Marker component attached to preview-queue block entities.
// Used to distinguish them from real game blocks so that:
//   - clearLines gravity (active==false guard) doesn't apply (preview blocks use active=true)
//   - game-over board wipe skips them, preserving the queue display
struct NextPieceBlockComponent {};
