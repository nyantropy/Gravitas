#pragma once
#include "GtsModelHandle.h"
#include "GtsModelDiagnostic.h"
#include <vector>
class GtsModelRequestResult
{
    public:
    bool succeeded() const
    {
        return static_cast<bool>(resource);
    }
    const GtsModelHandle& handle() const
    {
        return resource;
    }
    const std::vector<GtsModelDiagnostic>& diagnostics() const
    {
        return messages;
    }

    private:
    friend class GtsModelRegistry;
    GtsModelRequestResult(GtsModelHandle resource, std::vector<GtsModelDiagnostic> diagnostics);
    GtsModelHandle                  resource;
    std::vector<GtsModelDiagnostic> messages;
};
