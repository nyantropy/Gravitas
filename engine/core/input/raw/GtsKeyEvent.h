#pragma once
#include "GtsKey.h"

struct GtsKeyEvent
{
    GtsKey key;
    bool   pressed;
    int    mods = 0;
};
