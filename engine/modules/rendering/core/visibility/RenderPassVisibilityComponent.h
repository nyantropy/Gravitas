#pragma once

struct RenderPassVisibilityComponent
{
    bool renderScene = true;
    bool renderParticles = true;

    static RenderPassVisibilityComponent allVisible()
    {
        return {};
    }
};
