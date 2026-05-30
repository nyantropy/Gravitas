#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "EngineToolPanel.h"

namespace gts::tools
{
    class EngineToolRegistry
    {
    public:
        template<typename Panel, typename... Args>
        Panel& addPanel(Args&&... args)
        {
            auto panel = std::make_unique<Panel>(std::forward<Args>(args)...);
            Panel& ref = *panel;
            panels.push_back(std::move(panel));
            return ref;
        }

        size_t size() const
        {
            return panels.size();
        }

        bool empty() const
        {
            return panels.empty();
        }

        EngineToolPanel* at(size_t index)
        {
            return index < panels.size() ? panels[index].get() : nullptr;
        }

        const EngineToolPanel* at(size_t index) const
        {
            return index < panels.size() ? panels[index].get() : nullptr;
        }

        std::vector<std::unique_ptr<EngineToolPanel>>& all()
        {
            return panels;
        }

    private:
        std::vector<std::unique_ptr<EngineToolPanel>> panels;
    };
}
