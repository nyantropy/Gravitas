#pragma once

#include <vector>

#include "ToolPane.h"

namespace gts::tools
{
    class ToolWorkspaceLayout
    {
    public:
        std::vector<PaneDescriptor> apply(const std::vector<PaneDescriptor>& baseDescriptors,
                                          ToolWorkspace workspace) const
        {
            std::vector<PaneDescriptor> descriptors = baseDescriptors;
            for (PaneDescriptor& descriptor : descriptors)
                applyPaneRole(descriptor, workspace);
            return descriptors;
        }

    private:
        static void applyPaneRole(PaneDescriptor& descriptor, ToolWorkspace workspace)
        {
            switch (workspace)
            {
                case ToolWorkspace::World:
                    applyWorldRole(descriptor);
                    break;
                case ToolWorkspace::Particles:
                    applyParticleRole(descriptor);
                    break;
                case ToolWorkspace::Assets:
                    applyAssetRole(descriptor);
                    break;
            }
        }

        static void applyWorldRole(PaneDescriptor& descriptor)
        {
            switch (descriptor.id)
            {
                case ToolPaneId::WorldViewport:
                    descriptor.dockArea = ToolDockArea::Center;
                    descriptor.order = 0;
                    descriptor.visibleInWorld = true;
                    descriptor.preferredSizeInWorld = 0.0f;
                    descriptor.minimumSize = 0.0f;
                    break;

                case ToolPaneId::SceneBrowser:
                    descriptor.dockArea = ToolDockArea::Left;
                    descriptor.order = 0;
                    descriptor.visibleInWorld = true;
                    descriptor.preferredSizeInWorld = 0.0f;
                    break;

                case ToolPaneId::PropertyInspector:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 0;
                    descriptor.visibleInWorld = true;
                    descriptor.preferredSizeInWorld = 0.700f;
                    break;

                case ToolPaneId::Diagnostics:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 1;
                    descriptor.visibleInWorld = true;
                    descriptor.preferredSizeInWorld = 0.0f;
                    break;

                case ToolPaneId::CurveTimeline:
                    descriptor.dockArea = ToolDockArea::Bottom;
                    descriptor.order = 0;
                    descriptor.visibleInWorld = false;
                    break;

                default:
                    break;
            }
        }

        static void applyParticleRole(PaneDescriptor& descriptor)
        {
            switch (descriptor.id)
            {
                case ToolPaneId::ParticlePreviewViewport:
                    descriptor.dockArea = ToolDockArea::Center;
                    descriptor.order = 0;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSize = 0.0f;
                    descriptor.preferredSizeInParticles = 0.0f;
                    descriptor.minimumSize = 0.0f;
                    break;

                case ToolPaneId::WorldViewport:
                    descriptor.visibleInParticles = false;
                    break;

                case ToolPaneId::PropertyInspector:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 0;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.700f;
                    break;

                case ToolPaneId::Diagnostics:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 1;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.0f;
                    break;

                case ToolPaneId::EffectHierarchy:
                    descriptor.dockArea = ToolDockArea::Left;
                    descriptor.order = 0;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.780f;
                    break;

                case ToolPaneId::EmitterDetails:
                    descriptor.dockArea = ToolDockArea::Left;
                    descriptor.order = 1;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.220f;
                    break;

                case ToolPaneId::CurveTimeline:
                    descriptor.dockArea = ToolDockArea::Bottom;
                    descriptor.order = 0;
                    descriptor.visibleInParticles = false;
                    break;

                default:
                    break;
            }
        }

        static void applyAssetRole(PaneDescriptor& descriptor)
        {
            switch (descriptor.id)
            {
                case ToolPaneId::MenuBar:
                case ToolPaneId::WorkspaceTabs:
                case ToolPaneId::ToolToolbar:
                case ToolPaneId::StatusBar:
                    descriptor.visibleInAssets = true;
                    break;

                case ToolPaneId::AssetBrowser:
                    descriptor.dockArea = ToolDockArea::Left;
                    descriptor.order = 0;
                    descriptor.visibleInAssets = true;
                    descriptor.preferredSizeInAssets = 0.0f;
                    break;

                case ToolPaneId::AssetPreview:
                    descriptor.dockArea = ToolDockArea::Center;
                    descriptor.order = 0;
                    descriptor.visibleInAssets = true;
                    descriptor.preferredSizeInAssets = 0.0f;
                    descriptor.minimumSize = 0.0f;
                    break;

                case ToolPaneId::WorldViewport:
                    descriptor.visibleInAssets = false;
                    break;

                case ToolPaneId::PropertyInspector:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 0;
                    descriptor.visibleInAssets = true;
                    descriptor.preferredSizeInAssets = 0.700f;
                    break;

                case ToolPaneId::Diagnostics:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 1;
                    descriptor.visibleInAssets = true;
                    descriptor.preferredSizeInAssets = 0.0f;
                    break;

                default:
                    descriptor.visibleInAssets = false;
                    break;
            }
        }
    };
} // namespace gts::tools
