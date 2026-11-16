#pragma once
#include "GraphicsSettingsRequest.h"

namespace gts::rendering::detail
{
    class GraphicsSettingsRequestCompletion
    {
      public:
        explicit GraphicsSettingsRequestCompletion(std::shared_ptr<GraphicsSettingsRequestResult> result);
        ~GraphicsSettingsRequestCompletion();
        void publish(GraphicsSettingsRequestStatus status, std::string message,
                     GraphicsRuntimeState observed);

      private:
        std::shared_ptr<GraphicsSettingsRequestResult> result;
    };
} // namespace gts::rendering::detail
