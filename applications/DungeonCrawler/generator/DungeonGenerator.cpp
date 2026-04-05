#include "generator/DungeonGenerator.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <cstdio>
#include <cmath>

// ─── RNG ───────────────────────────────────────────────────────────────────
// Simple xorshift32 — deterministic, no dependencies.
uint32_t DungeonGenerator::rng()
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

// Uniform int in [lo, hi] (inclusive)
static int rngRange(uint32_t& s, int lo, int hi)
{
    uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    if (lo >= hi) return lo;
    return lo + static_cast<int>(x % static_cast<uint32_t>(hi - lo + 1));
}

// ─── Entry point ───────────────────────────────────────────────────────────
GeneratedFloor DungeonGenerator::generate(const DungeonSpec& specIn)
{
    spec  = &specIn;
    nodes.clear();

    // Seed the RNG
    seed = (specIn.seed != 0)
        ? specIn.seed
        : static_cast<uint32_t>(12345u + specIn.floorNumber * 7919u);

    // Initialise floor
    GeneratedFloor f;
    f.floorNumber = specIn.floorNumber;
    f.width       = specIn.width;
    f.height      = specIn.height;
    f.tiles.assign(static_cast<size_t>(specIn.width) * specIn.height, TileType::Wall);
    floor = &f;

    // Build BSP tree and carve rooms
    buildBSP(0, 0, specIn.width, specIn.height, 0);
    carveRooms(0);

    // Connect sibling subtrees
    // Walk the tree: for each internal node connect left-subtree's and right-subtree's leaf rooms
    // We do this by an iterative post-order pass
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
    {
        const BSPNode& n = nodes[i];
        if (!n.isLeaf())
            connectRooms(n.left, n.right);
    }

    // Place budgeted content (templates, enemies, treasure)
    placeBudgetedContent();

    // Set a stable player start before stair placement so stair heuristics have
    // a sensible anchor on every floor.
    f.playerStart = choosePlayerStart();

    // Place floor-local transitions according to the spec prepared by the run manager.
    placeStairs();

    // Prefer spawning near the incoming stair when present.
    if (f.hasStairUp())
    {
        f.playerStart = f.stairUpPos;
    }

    // Connectivity validation — flood-fill from playerStart and warn if the
    // floor has disconnected walkable regions (indicates broken corridor generation).
    {
        int reachable = 0;
        std::vector<bool> visited(static_cast<size_t>(f.width) * f.height, false);
        std::queue<glm::ivec2> q;
        q.push(f.playerStart);
        visited[f.playerStart.y * f.width + f.playerStart.x] = true;
        while (!q.empty())
        {
            glm::ivec2 cur = q.front(); q.pop();
            ++reachable;
            const int dx[4] = {1,-1,0,0};
            const int dz[4] = {0,0,1,-1};
            for (int d = 0; d < 4; ++d)
            {
                int nx = cur.x + dx[d], nz = cur.y + dz[d];
                if (f.inBounds(nx, nz) && !visited[nz * f.width + nx] && f.isWalkable(nx, nz))
                {
                    visited[nz * f.width + nx] = true;
                    q.push(glm::ivec2{nx, nz});
                }
            }
        }

        int total = 0;
        for (auto t : f.tiles)
            if (t != TileType::Wall) ++total;

        if (total > 0 && reachable < total / 2)
        {
            std::printf("WARNING: Floor %d has disconnected regions (%d/%d reachable)\n",
                        f.floorNumber, reachable, total);
        }
    }

    return f;
}

// ─── BSP ───────────────────────────────────────────────────────────────────
int DungeonGenerator::buildBSP(int x, int y, int w, int h, int depth)
{
    int idx = static_cast<int>(nodes.size());
    nodes.push_back({ x, y, w, h });

    splitNode(idx, depth);
    return idx;
}

void DungeonGenerator::splitNode(int idx, int depth)
{
    BSPNode& n = nodes[idx];

    const int minSplit = spec->minRoomSize * 2 + 4;

    // Stop conditions
    if (depth >= 6)             return;
    if (n.w < minSplit && n.h < minSplit) return;

    bool splitH; // split horizontally = cut along Y axis, creating top/bottom children
    if (n.w >= minSplit && n.h >= minSplit)
    {
        if      (n.w > static_cast<int>(n.h * 1.25f)) splitH = false; // wide → split vertically
        else if (n.h > static_cast<int>(n.w * 1.25f)) splitH = true;  // tall → split horizontally
        else splitH = (rng() % 2 == 0);
    }
    else if (n.w >= minSplit)   splitH = false;
    else                        splitH = true;

    if (splitH)
    {
        if (n.h < minSplit) return;
        // Split position in range [40%..60%] of height
        int lo = n.y + static_cast<int>(n.h * 0.4f);
        int hi = n.y + static_cast<int>(n.h * 0.6f);
        if (lo >= hi) return;
        int split = rngRange(seed, lo, hi);

        int leftIdx  = buildBSP(n.x, n.y,     n.w, split - n.y,           depth + 1);
        int rightIdx = buildBSP(n.x, split,    n.w, n.y + n.h - split,     depth + 1);
        // Re-fetch reference after potential reallocation
        nodes[idx].left  = leftIdx;
        nodes[idx].right = rightIdx;
    }
    else
    {
        if (n.w < minSplit) return;
        int lo = n.x + static_cast<int>(n.w * 0.4f);
        int hi = n.x + static_cast<int>(n.w * 0.6f);
        if (lo >= hi) return;
        int split = rngRange(seed, lo, hi);

        int leftIdx  = buildBSP(n.x,     n.y, split - n.x,           n.h, depth + 1);
        int rightIdx = buildBSP(split,    n.y, n.x + n.w - split,     n.h, depth + 1);
        nodes[idx].left  = leftIdx;
        nodes[idx].right = rightIdx;
    }
}

// ─── Room carving ──────────────────────────────────────────────────────────
void DungeonGenerator::carveRooms(int idx)
{
    BSPNode& n = nodes[idx];

    if (!n.isLeaf())
    {
        carveRooms(n.left);
        carveRooms(n.right);
        return;
    }

    // Leave a 1-tile border
    const int minSize = spec->minRoomSize;
    const int maxSize = std::min(spec->maxRoomSize, std::min(n.w - 2, n.h - 2));

    if (maxSize < minSize) return; // region too small

    int rw = rngRange(seed, minSize, maxSize);
    int rh = rngRange(seed, minSize, maxSize);

    // Random offset within the border
    int rx = rngRange(seed, n.x + 1, n.x + n.w - 1 - rw);
    int rz = rngRange(seed, n.y + 1, n.y + n.h - 1 - rh);

    // Clamp to floor bounds
    if (rx < 0) rx = 0;
    if (rz < 0) rz = 0;
    if (rx + rw > floor->width)  rw = floor->width  - rx;
    if (rz + rh > floor->height) rh = floor->height - rz;
    if (rw < 1 || rh < 1) return;

    n.roomX = rx;
    n.roomY = rz;
    n.roomW = rw;
    n.roomH = rh;

    for (int z = rz; z < rz + rh; ++z)
        for (int x = rx; x < rx + rw; ++x)
            floor->set(x, z, TileType::Floor);

    GeneratedRoom room;
    room.x      = rx;
    room.y      = rz;
    room.width  = rw;
    room.height = rh;
    floor->rooms.push_back(room);
}

// ─── Corridor helpers ──────────────────────────────────────────────────────
void DungeonGenerator::carveHCorridor(int x1, int x2, int z)
{
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; ++x)
        if (floor->inBounds(x, z))
            floor->set(x, z, TileType::Floor);
}

void DungeonGenerator::carveVCorridor(int x, int z1, int z2)
{
    if (z1 > z2) std::swap(z1, z2);
    for (int z = z1; z <= z2; ++z)
        if (floor->inBounds(x, z))
            floor->set(x, z, TileType::Floor);
}

glm::ivec2 DungeonGenerator::roomCenter(const BSPNode& node) const
{
    return {
        node.roomX + node.roomW / 2,
        node.roomY + node.roomH / 2
    };
}

void DungeonGenerator::connectRooms(int idxA, int idxB)
{
    // BFS to find the nearest leaf with a valid room in each subtree.
    // Descending only via `left` fails when the leftmost leaf has no room.
    auto findLeafWithRoom = [&](int start) -> int
    {
        std::queue<int> q;
        q.push(start);
        while (!q.empty())
        {
            int cur = q.front(); q.pop();
            const BSPNode& n = nodes[cur];
            if (n.isLeaf())
            {
                if (n.roomW > 0 && n.roomH > 0) return cur;
            }
            else
            {
                q.push(n.left);
                q.push(n.right);
            }
        }
        return -1;
    };

    int leafA = findLeafWithRoom(idxA);
    int leafB = findLeafWithRoom(idxB);

    if (leafA < 0 || leafB < 0) return;

    const BSPNode& a = nodes[leafA];
    const BSPNode& b = nodes[leafB];

    if (a.roomW == 0 || a.roomH == 0) return;
    if (b.roomW == 0 || b.roomH == 0) return;

    glm::ivec2 ca = roomCenter(a);
    glm::ivec2 cb = roomCenter(b);

    // Randomly choose L-shape direction
    if (rng() % 2 == 0)
    {
        carveHCorridor(ca.x, cb.x, ca.y);
        carveVCorridor(cb.x, ca.y, cb.y);
    }
    else
    {
        carveVCorridor(ca.x, ca.y, cb.y);
        carveHCorridor(ca.x, cb.x, cb.y);
    }
}

// ─── Budget content ────────────────────────────────────────────────────────
bool DungeonGenerator::tryPlaceTemplate(const RoomTemplate& tmpl, int leafIdx)
{
    BSPNode& leaf = nodes[leafIdx];
    if (leaf.roomW < tmpl.width || leaf.roomH < tmpl.height) return false;
    if (tmpl.tiles.size() != static_cast<size_t>(tmpl.width * tmpl.height)) return false;

    // Overwrite tiles in the top-left of the room
    for (int row = 0; row < tmpl.height; ++row)
    {
        for (int col = 0; col < tmpl.width; ++col)
        {
            int wx = leaf.roomX + col;
            int wz = leaf.roomY + row;
            if (!floor->inBounds(wx, wz)) continue;

            int val = tmpl.tiles[row * tmpl.width + col];
            switch (val)
            {
                case 0: floor->set(wx, wz, TileType::Floor);      break;
                case 1: floor->set(wx, wz, TileType::Wall);       break;
                case 2: floor->set(wx, wz, TileType::StairDown);  break;
                case 3: floor->set(wx, wz, TileType::StairUp);    break;
                case 4: floor->set(wx, wz, TileType::Treasure);   break;
                case 5: floor->set(wx, wz, TileType::EnemySpawn); break;
                default: break;
            }
        }
    }
    return true;
}

void DungeonGenerator::spendEnemyBudget()
{
    std::vector<glm::ivec2> eligible;
    eligible.reserve(256);

    for (int z = 0; z < floor->height; ++z)
    {
        for (int x = 0; x < floor->width; ++x)
        {
            if (!floor->isWalkable(x, z)) continue;
            const glm::ivec2 tilePos = {x, z};
            if (tilePos == floor->stairDownPos || tilePos == floor->stairUpPos) continue;
            eligible.push_back(tilePos);
        }
    }

    int count = std::min(spec->budget.enemyCount, static_cast<int>(eligible.size()));
    for (int i = 0; i < count && !eligible.empty(); ++i)
    {
        int j = rngRange(seed, i, static_cast<int>(eligible.size()) - 1);
        std::swap(eligible[i], eligible[j]);
        floor->enemySpawnPositions.push_back(tileToEnemySpawnPosition(eligible[i]));
    }
}

void DungeonGenerator::spendTreasureBudget()
{
    // Prefer tiles inside recorded rooms
    std::vector<glm::ivec2> candidates;
    candidates.reserve(64);

    for (const GeneratedRoom& r : floor->rooms)
    {
        for (int z = r.y; z < r.y + r.height; ++z)
            for (int x = r.x; x < r.x + r.width; ++x)
                if (floor->get(x, z) == TileType::Floor)
                    candidates.push_back({x, z});
    }

    if (candidates.empty()) return;

    int count = std::min(spec->budget.treasureCount, static_cast<int>(candidates.size()));
    for (int i = 0; i < count; ++i)
    {
        int j = rngRange(seed, i, static_cast<int>(candidates.size()) - 1);
        std::swap(candidates[i], candidates[j]);
        floor->set(candidates[i].x, candidates[i].y, TileType::Treasure);
        floor->treasureSpawns.push_back(candidates[i]);
    }
}

void DungeonGenerator::placeBudgetedContent()
{
    // Gather leaf indices
    std::vector<int> leaves;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        if (nodes[i].isLeaf() && nodes[i].roomW > 0)
            leaves.push_back(i);

    // Sort templates by cost descending
    std::vector<RoomTemplate> sorted = spec->templates;
    std::sort(sorted.begin(), sorted.end(),
        [](const RoomTemplate& a, const RoomTemplate& b) { return a.cost > b.cost; });

    int roomBudget = spec->budget.roomCost;
    for (const RoomTemplate& t : sorted)
    {
        if (t.cost > roomBudget) continue;

        // Find a leaf large enough
        for (int li : leaves)
        {
            if (tryPlaceTemplate(t, li))
            {
                roomBudget -= t.cost;
                break;
            }
        }
    }

    spendEnemyBudget();
    spendTreasureBudget();
}

// ─── Stairs ────────────────────────────────────────────────────────────────
void DungeonGenerator::placeStairs()
{
    const glm::ivec2 start = floor->playerStart;

    // ── StairUp ─────────────────────────────────────────────────────────
    if (spec->placeStairUp)
    {
        glm::ivec2 upPos;

        if (spec->requiredStairUpPos.x >= 0)
        {
            upPos = spec->requiredStairUpPos;
            // Ensure it is a floor tile and carve a path to it
            floor->set(upPos.x, upPos.y, TileType::Floor);
            carveCorridorToStair(upPos);
        }
        else
        {
            auto candidates = collectStairCandidates(start, 3.0f);
            upPos = weightedRandomPick(candidates);
        }

        floor->set(upPos.x, upPos.y, TileType::StairUp);
        floor->stairUpPos = upPos;
    }

    // ── StairDown ───────────────────────────────────────────────────────
    if (spec->placeStairDown)
    {
        const float minDist = static_cast<float>(
            std::min(spec->width, spec->height)) * 0.4f;

        auto candidates = collectStairCandidates(start, minDist);

        // Filter candidates too close to StairUp
        if (floor->hasStairUp())
        {
            const glm::ivec2& upPos = floor->stairUpPos;
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [&](const StairCandidate& c)
                    {
                        float dx = static_cast<float>(c.pos.x - upPos.x);
                        float dz = static_cast<float>(c.pos.y - upPos.y);
                        return std::sqrt(dx*dx + dz*dz) < 6.0f;
                    }),
                candidates.end());
        }

        if (candidates.empty())
            candidates = collectStairCandidates(start, 5.0f);

        glm::ivec2 downPos = weightedRandomPick(candidates);
        floor->set(downPos.x, downPos.y, TileType::StairDown);
        floor->stairDownPos = downPos;
    }
}

glm::ivec2 DungeonGenerator::choosePlayerStart() const
{
    if (!floor->rooms.empty())
    {
        const GeneratedRoom& r = floor->rooms[0];
        return { r.x + r.width / 2, r.y + r.height / 2 };
    }

    return {1, 1};
}

glm::vec3 DungeonGenerator::tileToEnemySpawnPosition(const glm::ivec2& tilePos)
{
    return {
        tilePos.x + 0.5f,
        0.25f,
        tilePos.y + 0.5f
    };
}

std::vector<DungeonGenerator::StairCandidate>
DungeonGenerator::collectStairCandidates(const glm::ivec2& avoidPos,
                                          float minDistance) const
{
    std::vector<StairCandidate> candidates;
    for (int z = 1; z < spec->height - 1; ++z)
    {
        for (int x = 1; x < spec->width - 1; ++x)
        {
            if (floor->get(x, z) != TileType::Floor) continue;

            float dx   = static_cast<float>(x - avoidPos.x);
            float dz   = static_cast<float>(z - avoidPos.y);
            float dist = std::sqrt(dx*dx + dz*dz);

            if (dist < minDistance) continue;

            StairCandidate c;
            c.pos    = {x, z};
            c.weight = dist * dist; // far tiles preferred
            candidates.push_back(c);
        }
    }
    return candidates;
}

glm::ivec2 DungeonGenerator::weightedRandomPick(
    const std::vector<StairCandidate>& candidates)
{
    if (candidates.empty())
        return {spec->width / 2, spec->height / 2};

    float total = 0.0f;
    for (const auto& c : candidates)
        total += c.weight;

    float r = (static_cast<float>(rng()) /
               static_cast<float>(std::numeric_limits<uint32_t>::max())) * total;

    float cumulative = 0.0f;
    for (const auto& c : candidates)
    {
        cumulative += c.weight;
        if (r <= cumulative)
            return c.pos;
    }
    return candidates.back().pos;
}

void DungeonGenerator::carveCorridorToStair(const glm::ivec2& stairPos)
{
    // Find nearest room centre to the required stair position
    glm::ivec2 nearest  = {spec->width / 2, spec->height / 2};
    float      bestDist = std::numeric_limits<float>::max();

    for (const auto& node : nodes)
    {
        if (!node.isLeaf() || node.roomW == 0) continue;
        glm::ivec2 center = roomCenter(node);
        float dx   = static_cast<float>(center.x - stairPos.x);
        float dz   = static_cast<float>(center.y - stairPos.y);
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist < bestDist)
        {
            bestDist = dist;
            nearest  = center;
        }
    }

    carveHCorridor(nearest.x, stairPos.x, nearest.y);
    carveVCorridor(stairPos.x, nearest.y, stairPos.y);
}
