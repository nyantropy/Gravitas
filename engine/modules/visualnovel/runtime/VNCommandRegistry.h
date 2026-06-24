#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "EcsControllerContext.hpp"
#include "VNStage.h"
#include "VNTypes.h"

namespace gts::vn
{
    class VNRuntime;

    struct VNCommandResult
    {
        bool complete = true;
        std::string jumpLabel;
        bool stopScript = false;
    };

    struct VNCommandExecutionContext
    {
        VNRuntime& runtime;
        VNStage& stage;
        const EcsControllerContext& frame;
    };

    using VNCommandHandler = std::function<VNCommandResult(const VNCommandExecutionContext&, const VNCommand&)>;

    class VNCommandRegistry
    {
    public:
        void registerCommand(const std::string& name, VNCommandHandler handler)
        {
            if (name.empty())
                return;

            handlers[name] = std::move(handler);
        }

        void unregisterCommand(const std::string& name)
        {
            handlers.erase(name);
        }

        bool hasCommand(const std::string& name) const
        {
            return handlers.find(name) != handlers.end();
        }

        VNCommandResult execute(const std::string& name,
                                const VNCommandExecutionContext& context,
                                const VNCommand& command) const
        {
            const auto it = handlers.find(name);
            if (it == handlers.end())
                return {};

            return it->second(context, command);
        }

    private:
        std::unordered_map<std::string, VNCommandHandler> handlers;
    };
} // namespace gts::vn
