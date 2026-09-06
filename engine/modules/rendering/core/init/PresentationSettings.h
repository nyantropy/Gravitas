#pragma once

enum class PresentModePreference
{
    // present without waiting for vertical refresh
    Immediate = 0,

    // wait for vertical refresh, but replace a pending frame with a newer one instead of building a queue
    Mailbox,

    // queue frames for vertical refresh, normal vsync behavior basically
    Fifo
};

// how completed frames are presented
struct PresentationSettings
{
    PresentModePreference mode = PresentModePreference::Fifo;
};
