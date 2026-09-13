#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GtsSkinPalette.h"

struct GtsSkeletonPose;
struct GtsSkinBinding;
struct GtsSkeletonAsset;

struct GtsSkinPaletteEvaluationError
{
    std::string code;
    std::string message;
    std::string location;
};

class GtsSkinPaletteEvaluationResult
{
    public:
    bool succeeded() const
    {
        return evaluatedPalette.has_value();
    }
    const GtsSkinPalette* palette() const
    {
        return evaluatedPalette ? &*evaluatedPalette : nullptr;
    }
    const std::vector<GtsSkinPaletteEvaluationError>& diagnostics() const
    {
        return errors;
    }

    private:
    friend GtsSkinPaletteEvaluationResult
    evaluateGtsSkinPalette(const GtsSkeletonPose&, const GtsSkinBinding&, const GtsSkeletonAsset&);
    GtsSkinPaletteEvaluationResult(std::optional<GtsSkinPalette>              palette,
                                   std::vector<GtsSkinPaletteEvaluationError> diagnostics)
        : evaluatedPalette(std::move(palette)), errors(std::move(diagnostics))
    {
    }
    std::optional<GtsSkinPalette>              evaluatedPalette;
    std::vector<GtsSkinPaletteEvaluationError> errors;
};

[[nodiscard]] GtsSkinPaletteEvaluationResult
evaluateGtsSkinPalette(const GtsSkeletonPose& pose, const GtsSkinBinding& binding, const GtsSkeletonAsset& skeleton);
