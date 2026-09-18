#pragma once

#include <utility>

#include "GtsRealizedModel.h"
#include "assets/model/GtsModelDiagnostic.h"

class GtsModelRealizationResult
{
    public:
    bool succeeded() const
    {
        return static_cast<bool>(realized);
    }
    const std::shared_ptr<const GtsRealizedModel>& model() const
    {
        return realized;
    }
    const std::vector<GtsModelDiagnostic>& diagnostics() const
    {
        return messages;
    }

    private:
    friend GtsModelRealizationResult realizeGtsModel(GtsModelHandle model);
    GtsModelRealizationResult(std::shared_ptr<const GtsRealizedModel> model,
                              std::vector<GtsModelDiagnostic>         diagnostics)
        : realized(std::move(model)), messages(std::move(diagnostics))
    {
    }
    std::shared_ptr<const GtsRealizedModel> realized;
    std::vector<GtsModelDiagnostic>         messages;
};

// Handles retain validated immutable definitions. No source IO, animation or GPU work.
[[nodiscard]] GtsModelRealizationResult realizeGtsModel(GtsModelHandle model);
