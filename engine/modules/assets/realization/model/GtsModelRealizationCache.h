#pragma once

#include <map>
#include "GtsModelRealization.h"

// Main-thread service. Retains successful derived geometry and warnings until shutdown.
class GtsModelRealizationCache
{
    public:
    GtsModelRealizationResult realize(const GtsModelHandle& model);
    std::size_t               size() const
    {
        return entries.size();
    }

    private:
    std::map<GtsModelHandle, GtsModelRealizationResult, std::owner_less<GtsModelHandle>> entries;
};
