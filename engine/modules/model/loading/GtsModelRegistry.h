#pragma once
#include "model/public/GtsModelRequestResult.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <vector>
#include <tuple>

#include "model/public/GtsModelHandle.h"
#include "model/public/GtsModelRequest.h"
#include "model/domain/model/GtsModelDiagnostic.h"

// synchronous, main-thread-only - successful entries remain until registry shutdown
class GtsModelRegistry
{
    public:
    GtsModelRegistry()                                   = default;
    GtsModelRegistry(const GtsModelRegistry&)            = delete;
    GtsModelRegistry& operator=(const GtsModelRegistry&) = delete;

    GtsModelRequestResult   requestModel(const std::filesystem::path& path);
    GtsModelRequestResult   requestModel(const GtsModelRequest& request);
    const GtsModelResource* lookup(const GtsModelHandle& handle) const;
    std::size_t             size() const
    {
        return entries.size();
    }

    private:
    struct Entry
    {
        GtsModelHandle                  resource;
        std::vector<GtsModelDiagnostic> diagnostics;
    };
    // A logical request can retain more than one representation. Policy is
    // re-evaluated before choosing any entry; source snapshots cannot bypass it.
    using EntryKey = std::tuple<std::filesystem::path, std::filesystem::path, bool>;
    std::map<EntryKey, Entry> entries;
};
