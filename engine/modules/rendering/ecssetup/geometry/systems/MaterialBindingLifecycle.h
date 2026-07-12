#pragma once

#include "BitmapFont.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"

namespace gts::rendering
{
    inline MaterialInstance makeWorldTextMaterialInstance(MaterialRuntime& runtime,
                                                          const WorldTextComponent& text,
                                                          texture_id_type atlasTexture)
    {
        MaterialInstance instance;
        instance.definition = runtime.defaultMaterial().valid()
            ? runtime.getInstance(runtime.defaultMaterial())->definition
            : MaterialDefinitionHandle{};
        instance.baseColor = text.tint;
        instance.baseColorTexture = MaterialTextureBinding::resolved(atlasTexture);
        instance.vertexColorOnly = false;
        instance.renderState.doubleSided = true;
        instance.renderState.depthWrite = true;
        instance.renderState.blendMode = MaterialBlendMode::Alpha;
        instance.renderState.alphaMode =
            alphaModeForBlendMode(MaterialBlendMode::Alpha, text.tint.a, true);
        return instance;
    }

    inline bool writeMaterialReference(ECSWorld& world,
                                       ECSWorld::EntityCommandBuffer& commands,
                                       Entity entity,
                                       MaterialInstanceHandle material)
    {
        const MaterialInstanceHandle resolved = registerMaterialUser(world, entity, material);
        const bool changed =
            !world.hasComponent<MaterialReferenceComponent>(entity)
            || world.getComponent<MaterialReferenceComponent>(entity).material != resolved;

        if (world.hasComponent<MaterialReferenceComponent>(entity))
            world.getComponent<MaterialReferenceComponent>(entity).material = resolved;
        else
            commands.addComponent<MaterialReferenceComponent>(entity, MaterialReferenceComponent{resolved});

        if (changed)
        {
            markMaterialRepresentationDirty(world, entity);
            queueRenderObjectRefresh(world, entity);
        }
        return changed;
    }

    inline void cleanupWorldTextMaterialBinding(ECSWorld& world,
                                                Entity entity,
                                                WorldTextRuntimeComponent& runtime,
                                                ECSWorld::EntityCommandBuffer& commands)
    {
        if (!runtime.materialInitialized)
            return;

        MaterialInstanceHandle material = runtime.material;
        unregisterMaterialUser(world, entity);
        materialRuntime(world).destroyInstance(material);

        if (world.hasComponent<MaterialReferenceComponent>(entity) &&
            world.getComponent<MaterialReferenceComponent>(entity).material == material)
        {
            commands.removeComponent<MaterialReferenceComponent>(entity);
        }

        runtime.material = {};
        runtime.materialInitialized = false;
        markMaterialRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }

    inline void syncWorldTextMaterialBinding(ECSWorld& world,
                                             Entity entity,
                                             IResourceProvider* resources,
                                             ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr || !world.hasComponent<WorldTextComponent>(entity))
            return;

        if (!world.hasComponent<WorldTextRuntimeComponent>(entity))
            return;

        WorldTextRuntimeComponent& runtime = world.getComponent<WorldTextRuntimeComponent>(entity);
        if (!worldTextMaterialDescriptorUsable(world, entity))
        {
            cleanupWorldTextMaterialBinding(world, entity, runtime, commands);
            return;
        }

        const WorldTextComponent& text = world.getComponent<WorldTextComponent>(entity);
        if (runtime.fontID == 0 || runtime.boundFontPath != text.fontPath)
        {
            runtime.fontID = resources->requestFont(text.fontPath);
            runtime.boundFontPath = text.fontPath;
        }

        const BitmapFont* font = resources->getFont(runtime.fontID);
        if (font == nullptr || font->atlasTexture == 0)
        {
            cleanupWorldTextMaterialBinding(world, entity, runtime, commands);
            return;
        }

        MaterialRuntime& materials = materialRuntime(world);
        MaterialInstance next = makeWorldTextMaterialInstance(materials, text, font->atlasTexture);
        MaterialInstanceHandle handle = runtime.material;
        bool descriptorChanged = false;

        if (!runtime.materialInitialized || !materials.isInstanceAlive(handle))
        {
            handle = materials.createInstance(next);
            runtime.material = handle;
            runtime.materialInitialized = true;
            descriptorChanged = true;
        }
        else
        {
            const MaterialInstance* current = materials.getInstance(handle);
            descriptorChanged =
                current == nullptr
                || current->baseColor != next.baseColor
                || current->baseColorTexture.resolvedTextureID != next.baseColorTexture.resolvedTextureID
                || current->renderState.alphaMode != next.renderState.alphaMode;
            if (descriptorChanged)
                materials.setInstance(handle, next);
        }

        writeMaterialReference(world, commands, entity, handle);
    }
}
