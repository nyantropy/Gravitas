#pragma once

struct GtsFrameStats;

// Optional scene participation, independent of the foundational scene lifecycle.
class ISceneFrameStats
{
    public:
    virtual ~ISceneFrameStats() = default;

    // Called after initial statistics are constructed, before frame extraction.
    virtual void populateFrameStats(GtsFrameStats& /*stats*/) const {}

    // Called after submission/final statistics, before profile accumulation.
    virtual void onFrameStats(const GtsFrameStats& /*stats*/) {}
};
