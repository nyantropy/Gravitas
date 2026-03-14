#pragma once

enum class WindowMode
{
    Windowed,             // Resizable window with title bar; freely draggable.
    BorderlessFullscreen, // Borderless window sized to the primary monitor;
                          // not draggable, does not minimise on focus loss.
    Fullscreen            // Exclusive fullscreen; minimises on focus loss.
};