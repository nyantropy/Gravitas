#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "model/loading/GtsPreparedModelDefinition.h"
#include "model/domain/model/GtsModelDiagnostic.h"

std::optional<GtsPreparedModelDefinition> loadGtsCookedModel(const std::filesystem::path&     path,
                                                             std::vector<GtsModelDiagnostic>& diagnostics);
