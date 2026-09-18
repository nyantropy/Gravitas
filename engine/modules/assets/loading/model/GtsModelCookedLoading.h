#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "assets/loading/model/GtsPreparedModelDefinition.h"
#include "assets/model/GtsModelDiagnostic.h"

std::optional<GtsPreparedModelDefinition> loadGtsCookedModel(const std::filesystem::path&     path,
                                                             std::vector<GtsModelDiagnostic>& diagnostics);
