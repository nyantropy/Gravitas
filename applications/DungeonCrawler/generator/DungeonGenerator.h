#pragma once

#include "DungeonSpec.h"
#include "GeneratedFloor.h"
#include <vector>

class DungeonGenerator
{
public:
    GeneratedFloor generate(const DungeonSpec& spec);

private:
    struct BSPNode
    {
        int x = 0, y = 0, w = 0, h = 0;
        int roomX = 0, roomY = 0, roomW = 0, roomH = 0;
        int left  = -1;
        int right = -1;

        bool isLeaf() const { return left == -1 && right == -1; }
    };

    std::vector<BSPNode> nodes;
    GeneratedFloor*      floor = nullptr;
    const DungeonSpec*   spec  = nullptr;
    uint32_t             seed  = 0;

    uint32_t   rng();
    int        buildBSP(int x, int y, int w, int h, int depth);
    void       splitNode(int idx, int depth);
    void       carveRooms(int idx);
    void       connectRooms(int idxA, int idxB);
    void       carveHCorridor(int x1, int x2, int z);
    void       carveVCorridor(int x, int z1, int z2);
    glm::ivec2 roomCenter(const BSPNode& node) const;
    void       placeBudgetedContent();
    bool       tryPlaceTemplate(const RoomTemplate& tmpl, int leafIdx);
    void       spendEnemyBudget();
    void       spendTreasureBudget();
    void       placeStairs();
    glm::ivec2 choosePlayerStart() const;

    struct StairCandidate {
        glm::ivec2 pos;
        float      weight = 0.0f;
    };

    std::vector<StairCandidate> collectStairCandidates(const glm::ivec2& avoidPos,
                                                        float minDistance) const;
    glm::ivec2 weightedRandomPick(const std::vector<StairCandidate>& candidates);
    void       carveCorridorToStair(const glm::ivec2& stairPos);
};
