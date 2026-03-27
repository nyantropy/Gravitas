#pragma once

#include <string>

enum class DungeonGameResult { Playing, Won, Lost };

// Singleton component — global dungeon game state.
// Written by CombatSystem and EnemySystem; read by HudSystem and all game systems.
struct DungeonGameStateComponent
{
    int  playerHp    = 5;
    int  playerMaxHp = 5;
    bool hasKey      = false;

    DungeonGameResult result = DungeonGameResult::Playing;

    // Temporary message shown at centre-screen for messageTimer seconds.
    std::string message      = "FIND THE KEY";
    float       messageTimer = 3.0f;

    float attackCooldown = 0.0f;  // prevents spam-attacking
    float hurtCooldown   = 0.0f;  // player invincibility window after taking damage

    static constexpr float ATTACK_COOLDOWN = 0.4f;
    static constexpr float HURT_COOLDOWN   = 0.8f;
};
