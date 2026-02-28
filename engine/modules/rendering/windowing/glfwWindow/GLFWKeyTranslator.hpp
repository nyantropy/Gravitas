#pragma once

#include <array>
#include <GLFW/glfw3.h>

#include "GtsKeyTranslator.hpp"

// GLFW-specific scancode translator.
//
// A single constexpr MAPPING table is the only source of truth.
// Both translation directions are derived from it at construction time,
// after glfwInit() has run, by calling glfwGetKeyScancode() for each entry.
//
// Hot-path lookups are O(1) array reads; no dynamic allocations occur during
// fromPlatformScancode() or toPlatformScancode().

class GLFWKeyTranslator : public GtsKeyTranslator
{
    // ── authoritative table ───────────────────────────────────────────────
    struct KeyMapping { GtsKey gtsKey; int glfwKey; };

    static constexpr KeyMapping MAPPING[] =
    {
        // Letters
        { GtsKey::A, GLFW_KEY_A }, { GtsKey::B, GLFW_KEY_B }, { GtsKey::C, GLFW_KEY_C },
        { GtsKey::D, GLFW_KEY_D }, { GtsKey::E, GLFW_KEY_E }, { GtsKey::F, GLFW_KEY_F },
        { GtsKey::G, GLFW_KEY_G }, { GtsKey::H, GLFW_KEY_H }, { GtsKey::I, GLFW_KEY_I },
        { GtsKey::J, GLFW_KEY_J }, { GtsKey::K, GLFW_KEY_K }, { GtsKey::L, GLFW_KEY_L },
        { GtsKey::M, GLFW_KEY_M }, { GtsKey::N, GLFW_KEY_N }, { GtsKey::O, GLFW_KEY_O },
        { GtsKey::P, GLFW_KEY_P }, { GtsKey::Q, GLFW_KEY_Q }, { GtsKey::R, GLFW_KEY_R },
        { GtsKey::S, GLFW_KEY_S }, { GtsKey::T, GLFW_KEY_T }, { GtsKey::U, GLFW_KEY_U },
        { GtsKey::V, GLFW_KEY_V }, { GtsKey::W, GLFW_KEY_W }, { GtsKey::X, GLFW_KEY_X },
        { GtsKey::Y, GLFW_KEY_Y }, { GtsKey::Z, GLFW_KEY_Z },

        // Digits
        { GtsKey::Digit0, GLFW_KEY_0 }, { GtsKey::Digit1, GLFW_KEY_1 },
        { GtsKey::Digit2, GLFW_KEY_2 }, { GtsKey::Digit3, GLFW_KEY_3 },
        { GtsKey::Digit4, GLFW_KEY_4 }, { GtsKey::Digit5, GLFW_KEY_5 },
        { GtsKey::Digit6, GLFW_KEY_6 }, { GtsKey::Digit7, GLFW_KEY_7 },
        { GtsKey::Digit8, GLFW_KEY_8 }, { GtsKey::Digit9, GLFW_KEY_9 },

        // Whitespace / editing
        { GtsKey::Space,     GLFW_KEY_SPACE     },
        { GtsKey::Enter,     GLFW_KEY_ENTER     },
        { GtsKey::Escape,    GLFW_KEY_ESCAPE    },
        { GtsKey::Backspace, GLFW_KEY_BACKSPACE },
        { GtsKey::Tab,       GLFW_KEY_TAB       },

        // Arrows
        { GtsKey::ArrowUp,    GLFW_KEY_UP    },
        { GtsKey::ArrowDown,  GLFW_KEY_DOWN  },
        { GtsKey::ArrowLeft,  GLFW_KEY_LEFT  },
        { GtsKey::ArrowRight, GLFW_KEY_RIGHT },

        // Modifiers
        { GtsKey::LeftShift,  GLFW_KEY_LEFT_SHIFT    },
        { GtsKey::RightShift, GLFW_KEY_RIGHT_SHIFT   },
        { GtsKey::LeftCtrl,   GLFW_KEY_LEFT_CONTROL  },
        { GtsKey::RightCtrl,  GLFW_KEY_RIGHT_CONTROL },
        { GtsKey::LeftAlt,    GLFW_KEY_LEFT_ALT      },
        { GtsKey::RightAlt,   GLFW_KEY_RIGHT_ALT     },

        // Function keys
        { GtsKey::F1,  GLFW_KEY_F1  }, { GtsKey::F2,  GLFW_KEY_F2  },
        { GtsKey::F3,  GLFW_KEY_F3  }, { GtsKey::F4,  GLFW_KEY_F4  },
        { GtsKey::F5,  GLFW_KEY_F5  }, { GtsKey::F6,  GLFW_KEY_F6  },
        { GtsKey::F7,  GLFW_KEY_F7  }, { GtsKey::F8,  GLFW_KEY_F8  },
        { GtsKey::F9,  GLFW_KEY_F9  }, { GtsKey::F10, GLFW_KEY_F10 },
        { GtsKey::F11, GLFW_KEY_F11 }, { GtsKey::F12, GLFW_KEY_F12 },
    };

    // ── derived lookup tables (built once after glfwInit) ─────────────────
    // Scancodes are platform-specific but fit comfortably within 512 on all
    // supported backends (X11: 8–255, Windows: 0–127/255, evdev: < 512).
    static constexpr size_t MAX_SCANCODE = 512;
    static constexpr size_t KEY_COUNT    = static_cast<size_t>(GtsKey::COUNT);

    std::array<GtsKey, MAX_SCANCODE> scancodeToKey{};  // scancode → GtsKey
    std::array<int,    KEY_COUNT>    keyToScancode{};  // GtsKey   → scancode

public:
    // Must be constructed after glfwInit() so that glfwGetKeyScancode() works.
    GLFWKeyTranslator()
    {
        scancodeToKey.fill(GtsKey::Unknown);
        keyToScancode.fill(-1);

        for (const auto& entry : MAPPING)
        {
            int sc = glfwGetKeyScancode(entry.glfwKey);
            if (sc >= 0 && sc < static_cast<int>(MAX_SCANCODE))
            {
                scancodeToKey[static_cast<size_t>(sc)]                      = entry.gtsKey;
                keyToScancode[static_cast<size_t>(entry.gtsKey)] = sc;
            }
        }
    }

    // ── GtsKeyTranslator interface ────────────────────────────────────────

    GtsKey fromPlatformScancode(int scancode) const override
    {
        if (scancode < 0 || scancode >= static_cast<int>(MAX_SCANCODE))
            return GtsKey::Unknown;
        return scancodeToKey[static_cast<size_t>(scancode)];
    }

    int toPlatformScancode(GtsKey key) const override
    {
        size_t idx = static_cast<size_t>(key);
        if (idx >= KEY_COUNT)
            return -1;
        return keyToScancode[idx];
    }
};
