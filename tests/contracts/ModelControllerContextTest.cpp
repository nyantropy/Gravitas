#include "GtsModelControllerContext.h"
#include "ECSWorld.hpp"

#if __has_include("RenderingControllerContext.h") || __has_include("UiControllerContext.h") ||                         \
                                                                   __has_include("PhysicsControllerContext.h")
#error "Model controller contracts must not publish other controller capabilities"
#endif

class ModelReader : public ECSControllerSystem
{
    public:
    explicit ModelReader(bool& missing) : missing(missing) {}
    void update(const EcsControllerContext& ctx) override
    {
        const auto& model = gts::model::controllerContext(ctx);
        missing           = model.models == nullptr && model.modelRealizations == nullptr;
    }

    private:
    bool& missing;
};

int main()
{
    ECSWorld world;
    bool     missing = false;
    world.addControllerSystem<ModelReader>(static_cast<EcsSystemGroup>(1ull), missing);
    world.updateControllers(EcsControllerContext{world});
    return missing ? 0 : 1;
}
