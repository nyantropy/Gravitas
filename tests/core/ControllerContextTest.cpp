#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    struct CallData
    {
        int                  value = 0;
        std::shared_ptr<int> lifetime;
    };

    struct OtherData
    {
        int value = 0;
    };

    class Reader : public ECSControllerSystem
    {
        public:
        Reader(std::vector<int>& calls, int order) : calls(calls), order(order) {}
        void update(const EcsControllerContext& ctx) override
        {
            require(ctx.time && ctx.time->deltaTime == 0.25f, "Timing must reach controllers unchanged");
            require(ctx.input && ctx.engineCommands, "Foundational services must reach controllers");
            calls.push_back(order + ctx.frameData.get<CallData>().value);
        }

        private:
        std::vector<int>& calls;
        int               order;
    };
} // namespace

int main()
{
    ECSWorld    world;
    ECSWorld    otherWorld;
    TimeContext time;
    time.deltaTime = 0.25f;
    InputBindingRegistry input;
    GtsCommandBuffer     commands;
    std::vector<int>     calls;
    world.addControllerSystem<Reader>(static_cast<EcsSystemGroup>(1ull), calls, 1);
    world.addControllerSystem<Reader>(static_cast<EcsSystemGroup>(1ull), calls, 2);
    std::weak_ptr<int> lifetime;
    {
        EcsControllerContext ctx{world};
        ctx.time           = &time;
        ctx.input          = &input;
        ctx.engineCommands = &commands;
        world.updateControllers(ctx);
        require(calls == std::vector<int>({1, 2}), "Core-only controllers execute in registration order");

        auto& data                            = ctx.frameData.edit<CallData>();
        data.value                            = 10;
        data.lifetime                         = std::make_shared<int>(7);
        lifetime                              = data.lifetime;
        ctx.frameData.edit<OtherData>().value = 99;
        require(&data == &ctx.frameData.get<CallData>(), "Adding a payload must preserve existing references");
        EcsControllerContext copy             = ctx;
        copy.frameData.edit<CallData>().value = 20;
        require(data.value == 10, "Context copies must keep independent value snapshots");
        require(EcsControllerContext{world}.frameData.get<CallData>().value == 0,
                "Simultaneous contexts for one world must not share call data");
        require(EcsControllerContext{otherWorld}.frameData.get<CallData>().value == 0, "Worlds must remain isolated");
        world.updateControllers(ctx);
        world.updateControllers(copy);
        require(calls == std::vector<int>({1, 2, 11, 12, 21, 22}), "Each call sees its own snapshot");
    }
    require(lifetime.expired(), "Call data must be released when contexts expire");
    world.clear();
    require(EcsControllerContext{world}.frameData.get<CallData>().value == 0,
            "Reset worlds must not observe old call data");
}
