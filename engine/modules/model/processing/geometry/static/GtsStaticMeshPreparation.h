#pragma once

#include <optional>
#include <vector>

#include "GtsPreparedStaticMesh.h"
#include "GtsModelDiagnostic.h"

struct GtsModelMesh;

class GtsStaticMeshPreparationResult
{
    public:
    bool succeeded() const
    {
        return preparedMesh.has_value();
    }
    const GtsPreparedStaticMesh* mesh() const
    {
        return preparedMesh ? &*preparedMesh : nullptr;
    }
    const std::vector<GtsModelDiagnostic>& diagnostics() const
    {
        return messages;
    }
    bool hasWarnings() const;

    private:
    friend GtsStaticMeshPreparationResult prepareGtsStaticMesh(const GtsModelMesh& mesh);
    GtsStaticMeshPreparationResult(std::optional<GtsPreparedStaticMesh> mesh,
                                   std::vector<GtsModelDiagnostic>      diagnostics);

    std::optional<GtsPreparedStaticMesh> preparedMesh;
    std::vector<GtsModelDiagnostic>      messages;
};

[[nodiscard]] GtsStaticMeshPreparationResult prepareGtsStaticMesh(const GtsModelMesh& mesh);
