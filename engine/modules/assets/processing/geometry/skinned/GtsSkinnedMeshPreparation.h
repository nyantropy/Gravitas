#pragma once

#include <optional>
#include <vector>

#include "GtsPreparedSkinnedMesh.h"
#include "assets/model/GtsModelDiagnostic.h"

struct GtsModelMesh;
struct GtsSkinBinding;

class GtsSkinnedMeshPreparationResult
{
    public:
    bool succeeded() const
    {
        return preparedMesh.has_value();
    }
    const GtsPreparedSkinnedMesh* mesh() const
    {
        return preparedMesh ? &*preparedMesh : nullptr;
    }
    const std::vector<GtsModelDiagnostic>& diagnostics() const
    {
        return messages;
    }
    bool hasWarnings() const;

    private:
    friend GtsSkinnedMeshPreparationResult prepareGtsSkinnedMesh(const GtsModelMesh&, const GtsSkinBinding&);
    GtsSkinnedMeshPreparationResult(std::optional<GtsPreparedSkinnedMesh> mesh,
                                    std::vector<GtsModelDiagnostic>       diagnostics);
    std::optional<GtsPreparedSkinnedMesh> preparedMesh;
    std::vector<GtsModelDiagnostic>       messages;
};

// binding context is mandatory, while actual skeleton-use compatibility remains model validations responsibility
[[nodiscard]] GtsSkinnedMeshPreparationResult prepareGtsSkinnedMesh(const GtsModelMesh&   mesh,
                                                                    const GtsSkinBinding& binding);
