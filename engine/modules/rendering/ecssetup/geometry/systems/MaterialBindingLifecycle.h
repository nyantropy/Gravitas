#pragma once

#include "BitmapFont.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "LegacyMaterialRuntimeComponent.h"
#include "MaterialComponent.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"

namespace gts::rendering
{
    inline bool materialDescriptorDiffers(const MaterialComponent& lhs,
                                          const MaterialComponent& rhs)
    {
        return lhs.texturePath != rhs.texturePath
            || lhs.tint != rhs.tint
            || lhs.blendMode != rhs.blendMode
            || lhs.doubleSided != rhs.doubleSided
            || lhs.vertexColorOnly != rhs.vertexColorOnly
            || lhs.depthWrite != rhs.depthWrite;
    }

    inline MaterialInstance makeLegacyMaterialInstance(MaterialRuntime& runtime,
                                                       const MaterialComponent& material)
    {
        MaterialInstance instance;
        instance.definition = runtime.defaultMaterial().valid()
            ? runtime.getInstance(runtime.defaultMaterial())->definition
            : MaterialDefinitionHandle{};
        instance.baseColor = material.tint;
        instance.baseColorTexture = MaterialTextureBinding::assetPath(material.texturePath);
        instance.vertexColorOnly = material.vertexColorOnly;
        instance.renderState.doubleSided = material.doubleSided;
        instance.renderState.depthWrite = material.depthWrite;
        instance.renderState.legacyBlendMode = material.blendMode;
        instance.renderState.alphaMode =
            alphaModeForLegacyMaterial(material.blendMode, material.tint.a, material.depthWrite);
        return instance;
    }

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
        instance.renderState.legacyBlendMode = MaterialBlendMode::Alpha;
        instance.renderState.alphaMode =
            alphaModeForLegacyMaterial(MaterialBlendMode::Alpha, text.tint.a, true);
        return instance;
    }

    inline void writeMaterialReference(ECSWorld& world,
                                       ECSWorld::EntityCommandBuffer& commands,
                                       Entity entity,
                                       MaterialInstanceHandle material)
    {
        if (world.hasComponent<MaterialReferenceComponent>(entity))
            world.getComponent<MaterialReferenceComponent>(entity).material = material;
        else
            commands.addComponent<MaterialReferenceComponent>(entity, MaterialReferenceComponent{material});
    }

    inline void cleanupLegacyMaterialBinding(ECSWorld& world,
                                             Entity entity,
                                             ECSWorld::EntityCommandBuffer& commands)
    {
        if (!world.hasComponent<LegacyMaterialRuntimeComponent>(entity))
            return;

        LegacyMaterialRuntimeComponent& runtime = world.getComponent<LegacyMaterialRuntimeComponent>(entity);
        MaterialInstanceHandle material = runtime.material;
        materialRuntime(world).destroyInstance(material);

        if (world.hasComponent<MaterialReferenceComponent>(entity) &&
            world.getComponent<MaterialReferenceComponent>(entity).material == material)
        {
            commands.removeComponent<MaterialReferenceComponent>(entity);
        }

        commands.removeComponent<LegacyMaterialRuntimeComponent>(entity);
        markMaterialRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }

    inline void cleanupWorldTextMaterialBinding(ECSWorld& world,
                                                Entity entity,
                                                WorldTextRuntimeComponent& runtime,
                                                ECSWorld::EntityCommandBuffer& commands)
    {
        if (!runtime.materialInitialized)
            return;

        MaterialInstanceHandle material = runtime.material;
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

    inline void syncLegacyMaterialBinding(ECSWorld& world,
                                          Entity entity,
                                          IResourceProvider* resources,
                                          ECSWorld::EntityCommandBuffer& commands)
    {
        MaterialRuntime& materials = materialRuntime(world);
        if (!world.hasComponent<MaterialComponent>(entity) ||
            !legacyMaterialDescriptorUsable(world, entity))
        {
            cleanupLegacyMaterialBinding(world, entity, commands);
            return;
        }

        const MaterialComponent& material = world.getComponent<MaterialComponent>(entity);
        MaterialInstanceHandle handle;
        bool descriptorChanged = true;

        if (world.hasComponent<LegacyMaterialRuntimeComponent>(entity))
        {
            LegacyMaterialRuntimeComponent& runtime = world.getComponent<LegacyMaterialRuntimeComponent>(entity);
            handle = runtime.material;
            descriptorChanged =
                !runtime.initialized || materialDescriptorDiffers(runtime.lastDescriptor, material);

            if (!materials.isInstanceAlive(handle))
            {
                handle = materials.createInstance(makeLegacyMaterialInstance(materials, material));
                runtime.material = handle;
                runtime.lastDescriptor = material;
                runtime.initialized = true;
                descriptorChanged = true;
            }
            else if (descriptorChanged)
            {
                materials.setInstance(handle, makeLegacyMaterialInstance(materials, material));
                runtime.lastDescriptor = material;
                runtime.initialized = true;
            }
        }
        else
        {
            handle = materials.createInstance(makeLegacyMaterialInstance(materials, material));
            commands.addComponent<LegacyMaterialRuntimeComponent>(
                entity,
                LegacyMaterialRuntimeComponent{handle, material, true});
        }

        writeMaterialReference(world, commands, entity, handle);
        const MaterialSyncResult result = materials.synchronizeGpuState(handle, resources);
        if (descriptorChanged || result.changed)
        {
            markMaterialRepresentationDirty(world, entity);
            queueRenderObjectRefresh(world, entity);
        }
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
        const MaterialSyncResult result = materials.synchronizeGpuState(handle, resources);
        if (descriptorChanged || result.changed)
        {
            markMaterialRepresentationDirty(world, entity);
            queueRenderObjectRefresh(world, entity);
        }
    }
}
