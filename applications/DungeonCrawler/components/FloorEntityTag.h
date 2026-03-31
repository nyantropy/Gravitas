#pragma once

// Marker component attached to every entity that belongs to a specific dungeon
// floor (tiles, stairs, enemies, walls, ceiling).  When the player transitions
// to a new floor, DungeonFloorScene::swapToFloor() collects all tagged entities,
// releases their GPU resources, and destroys them before spawning the next floor.
//
// Entities that must persist across floor transitions (player, debug camera) must
// NOT receive this tag.
struct FloorEntityTag {};
