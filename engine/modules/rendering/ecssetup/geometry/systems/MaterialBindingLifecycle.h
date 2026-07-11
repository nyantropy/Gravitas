#pragma once

#include "BitmapFont.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MaterialComponent.h"
#include "MaterialGpuComponent.h"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"

namespace gts::rendering
{
    inline bool materialGpuDiffers(const MaterialGpuComponent& lhs, const MaterialGpuComponent& rhs)
    {
        return lhs.textureID != rhs.textureID
            || lhs.tint != rhs.tint
            || lhs.blendMode != rhs.blendMode
            || lhs.doubleSided != rhs.doubleSided
            || lhs.vertexColorOnly != rhs.vertexColorOnly
            || lhs.depthWrite != rhs.depthWrite
            || lhs.boundTexturePath != rhs.boundTexturePath;
    }

    inline void writeMaterialGpu(ECSWorld& world,
                                 ECSWorld::EntityCommandBuffer& commands,
                                 Entity entity,
                                 const MaterialGpuComponent& materialGpu)
    {
        if (world.hasComponent<MaterialGpuComponent>(entity))
            world.getComponent<MaterialGpuComponent>(entity) = materialGpu;
        else
            commands.addComponent<MaterialGpuComponent>(entity, materialGpu);
    }

    inline void syncLegacyMaterialBinding(ECSWorld& world,
                                          Entity entity,
                                          IResourceProvider* resources,
                                          ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr || !world.hasComponent<MaterialComponent>(entity))
            return;

        if (!legacyMaterialDescriptorUsable(world, entity))
        {
            scheduleMaterialGpuCleanup(world, commands, entity);
            queueRenderObjectRefresh(world, entity);
            return;
        }

        const MaterialComponent& material = world.getComponent<MaterialComponent>(entity);
        MaterialGpuComponent next = world.hasComponent<MaterialGpuComponent>(entity)
            ? world.getComponent<MaterialGpuComponent>(entity)
            : MaterialGpuComponent{};

        if (next.textureID == 0 || next.boundTexturePath != material.texturePath)
            next.textureID = resources->requestTexture(material.texturePath);
        next.tint = material.tint;
        next.blendMode = material.blendMode;
        next.doubleSided = material.doubleSided;
        next.vertexColorOnly = material.vertexColorOnly;
        next.depthWrite = material.depthWrite;
        next.boundTexturePath = material.texturePath;

        const bool changed =
            !world.hasComponent<MaterialGpuComponent>(entity)
            || materialGpuDiffers(world.getComponent<MaterialGpuComponent>(entity), next);
        if (!changed)
            return;

        writeMaterialGpu(world, commands, entity, next);
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

        if (!worldTextMaterialDescriptorUsable(world, entity)
            || !world.hasComponent<WorldTextRuntimeComponent>(entity))
        {
            scheduleMaterialGpuCleanup(world, commands, entity);
            queueRenderObjectRefresh(world, entity);
            return;
        }

        WorldTextRuntimeComponent& runtime = world.getComponent<WorldTextRuntimeComponent>(entity);
        const WorldTextComponent& text = world.getComponent<WorldTextComponent>(entity);
        if (runtime.fontID == 0 || runtime.boundFontPath != text.fontPath)
        {
            runtime.fontID = resources->requestFont(text.fontPath);
            runtime.boundFontPath = text.fontPath;
        }

        const BitmapFont* font = resources->getFont(runtime.fontID);
        if (font == nullptr || font->atlasTexture == 0)
        {
            scheduleMaterialGpuCleanup(world, commands, entity);
            queueRenderObjectRefresh(world, entity);
            return;
        }

        MaterialGpuComponent next = world.hasComponent<MaterialGpuComponent>(entity)
            ? world.getComponent<MaterialGpuComponent>(entity)
            : MaterialGpuComponent{};
        next.textureID = font->atlasTexture;
        next.tint = text.tint;
        next.blendMode = MaterialBlendMode::Alpha;
        next.doubleSided = true;
        next.vertexColorOnly = false;
        next.depthWrite = true;
        next.boundTexturePath = text.fontPath;

        const bool changed =
            !world.hasComponent<MaterialGpuComponent>(entity)
            || materialGpuDiffers(world.getComponent<MaterialGpuComponent>(entity), next);
        if (!changed)
            return;

        writeMaterialGpu(world, commands, entity, next);
        markMaterialRepresentationDirty(world, entity);
        queueRenderObjectRefresh(world, entity);
    }
}
