#pragma once

#include "GraphicsRuntimeState.h"
#include <memory>
#include <optional>
#include <string>

class GtsCommandBuffer;

namespace gts::rendering
{
    enum class GraphicsSettingsRequestStatus
    {
        Queued,
        Pending,
        Applied,
        Rejected,
        Superseded,
        Abandoned
    };

    struct GraphicsSettingsRequestResult
    {
        GraphicsSettingsRequestStatus status = GraphicsSettingsRequestStatus::Queued;
        GraphicsSettings requested;
        std::string message;
        std::optional<GraphicsRuntimeState> observed;
    };

    // Shared observation only; read on the engine/controller thread. Never
    // retain a borrowed controller service to observe a request on later frames.
    using GraphicsSettingsRequest = std::shared_ptr<const GraphicsSettingsRequestResult>;

    GraphicsSettingsRequest requestApplyGraphicsSettings(GtsCommandBuffer& commands,
                                                         const GraphicsSettings& settings);
} // namespace gts::rendering
