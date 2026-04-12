#pragma once

struct RenderDirtyComponent
{
    bool transformDirty = true;
    bool materialDirty  = true;
    bool meshDirty      = true;
};
