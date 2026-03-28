#pragma once

enum class GtsKey
{
    Unknown = 0,

    // Letters A–Z
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits 0–9
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,

    // Whitespace / editing
    Space, Enter, Escape, Backspace, Tab,

    // Arrow keys
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,

    // Modifiers
    LeftShift, RightShift,
    LeftCtrl,  RightCtrl,
    LeftAlt,   RightAlt,

    // Function keys
    F1, F2, F3, F4, F5,  F6,
    F7, F8, F9, F10, F11, F12,

    COUNT
};
