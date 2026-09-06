#pragma once

#include <cstdint>
#include <memory>

#include "IEngineModule.h"

struct EcsControllerContext;

namespace gts::tools
{
    struct ToolStartupOptions;

    // Engine-owned tooling runner that keeps editor state outside individual scenes.
    class EngineToolRuntime : public IEngineModule
    {
        public:
        EngineToolRuntime();
        ~EngineToolRuntime() override;

        const char* name() const override;
        void        registerInputBindings(InputBindingRegistry& input) override;
        void        prepare(const EcsControllerContext& ctx);
        void        update(const EcsControllerContext& ctx);
        uint32_t    controllerSystemCount() const;
        void        shutdown();
        void        setStartupOptions(ToolStartupOptions options);

        private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };
} // namespace gts::tools
