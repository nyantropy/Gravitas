#include "GraphicsSettingsRequestCompletion.h"
#include <utility>

namespace gts::rendering::detail
{
    GraphicsSettingsRequestCompletion::GraphicsSettingsRequestCompletion(
        std::shared_ptr<GraphicsSettingsRequestResult> result)
        : result(std::move(result))
    {
    }

    GraphicsSettingsRequestCompletion::~GraphicsSettingsRequestCompletion()
    {
        // No allocations or callbacks at teardown. Observers can outlive both
        // the command writer and renderer without being stuck in a pending state.
        if (result->status == GraphicsSettingsRequestStatus::Queued ||
            result->status == GraphicsSettingsRequestStatus::Pending)
            result->status = GraphicsSettingsRequestStatus::Abandoned;
    }

    void GraphicsSettingsRequestCompletion::publish(GraphicsSettingsRequestStatus status, std::string message,
                                                    GraphicsRuntimeState observed)
    {
        result->message = std::move(message);
        result->observed = std::move(observed);
        result->status = status;
    }
} // namespace gts::rendering::detail
