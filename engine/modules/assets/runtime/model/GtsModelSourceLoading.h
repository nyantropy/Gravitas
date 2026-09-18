#pragma once

#include <filesystem>

class GtsModelImportResult;

// Internal canonical source dispatch; callers request models through the registry.
GtsModelImportResult loadGtsModelSource(const std::filesystem::path& path);
