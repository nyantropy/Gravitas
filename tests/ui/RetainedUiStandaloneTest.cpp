#include "UiSurface.h"
#include "UiControllerContext.h"
#include "ECSWorld.hpp"

#include <cstdlib>
#include <iostream>

#if __has_include("UiSystem.h") || __has_include("IResourceProvider.hpp") || __has_include("PhysicsWorld.h")
#error "Retained UI must not expose rendering or physics implementation"
#endif

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    ECSWorld world;
    const EcsControllerContext context{world};
    require(gts::ui::controllerContext(context).ui == nullptr, "Absent UI access remains optional");
    UiSurfaceDesc desc;
    desc.rect = {0.25f, 0.25f, 0.5f, 0.5f};
    UiSurface surface(UI_DEFAULT_SURFACE, desc);
    require(surface.participatesInInput(), "Visible surface participates in input");
    UiInputFrame input;
    input.pointerX = 0.5f;
    input.pointerY = 0.5f;
    const auto local = surface.toLocalInput(input);
    require(local.pointerX == 0.5f && local.pointerY == 0.5f, "Surface converts input coordinates");

    auto& document = surface.document();
    const auto initialCount = document.getNodeCount();
    const auto node = document.createNode(UiNodeType::Image);
    UiImageData image;
    image.textureID = 37;
    require(document.setPayload(node, image), "Retained image accepts a resource handle");
    document.updateLayout(640.0f, 480.0f);
    document.rebuildVisualList();
    require(document.getNodeCount() == initialCount + 1, "Document owns the authored node");
    require(std::get<UiImageData>(document.findNode(node)->payload).textureID == 37,
            "Resource handle survives layout and visual extraction");
    require(document.removeNode(node), "Document releases authored nodes");
    surface.clear();
    require(document.getNodeCount() == initialCount, "Surface reset restores its roots");
    desc.inputEnabled = false;
    surface.setDesc(desc);
    require(!surface.participatesInInput() && surface.participatesInRendering(),
            "Input participation remains independent of rendering participation");
}
