#include "GtsModelRegistry.h"
#include "ScopedRuntimeAssetPolicy.h"

#include <fstream>
#include <stdexcept>

int main()
{
#if defined(GTS_SHIPPING_BUILD) || defined(GTS_DISABLE_RUNTIME_SOURCE_ASSET_FALLBACK)
    ScopedRuntimeAssetPolicy policy("development");
#else
    ScopedRuntimeAssetPolicy policy("strict");
#endif
    const auto path = std::filesystem::temp_directory_path() / "gravitas-strict-model-source.obj";
    std::ofstream(path) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    GtsModelRegistry registry;
    const auto       result = registry.requestModel(path);
    std::filesystem::remove(path);
    if (result.succeeded() || result.handle() || registry.size() != 0 || result.diagnostics().empty() ||
        result.diagnostics().back().code != "model.request.cooked_required")
    {
        throw std::runtime_error("Strict policy must reject source before importing or publishing it");
    }
}
