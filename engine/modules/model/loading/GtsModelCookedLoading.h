#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "GtsPreparedModelDefinition.h"
#include "GtsModelDiagnostic.h"

std::optional<GtsPreparedModelDefinition> loadGtsCookedModel(const std::filesystem::path&     path,
                                                             std::vector<GtsModelDiagnostic>& diagnostics);
