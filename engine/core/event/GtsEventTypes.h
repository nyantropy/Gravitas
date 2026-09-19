#pragma once

struct GtsWindowResizeEvent
{
    int width  = 0;
    int height = 0;
};

struct GtsMouseButtonEvent
{
    int  button  = 0;
    bool pressed = false;
    int  mods    = 0;
};

struct GtsCursorPositionEvent
{
    double x = 0.0;
    double y = 0.0;
};

struct GtsScrollEvent
{
    double x = 0.0;
    double y = 0.0;
};
