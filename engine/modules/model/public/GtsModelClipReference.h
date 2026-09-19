#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "GtsModelDiagnostic.h"
class GtsModelResource;
// non-owning, scoped to a resource retained by the registry or a model handle
class GtsModelClipReference
{
    public:
    uint32_t index() const
    {
        return clipIndex;
    }
    const GtsModelResource* model() const
    {
        return owner;
    }
    bool operator==(const GtsModelClipReference&) const = default;

    private:
    friend class GtsModelResource;
    GtsModelClipReference(const GtsModelResource* owner, uint32_t index) : owner(owner), clipIndex(index) {}
    const GtsModelResource* owner;
    uint32_t                clipIndex;
};

struct GtsModelClipLookupResult
{
    std::optional<GtsModelClipReference> reference;
    std::vector<GtsModelDiagnostic>      diagnostics;
    bool                                 succeeded() const
    {
        return reference.has_value();
    }
};
