#pragma once

#include <string>

// we can emit warnings or errors
enum class GtsModelDiagnosticSeverity
{
    Warning,
    Error
};

// a small struct for diagnostics when importing models from file formats
struct GtsModelDiagnostic
{
    GtsModelDiagnosticSeverity severity = GtsModelDiagnosticSeverity::Error;
    std::string code;
    std::string message;
    // canonical location (e.g. meshes[0].primitives[1]) or importer source location
    std::string location;
};
