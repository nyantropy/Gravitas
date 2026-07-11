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
            if (workspace == ToolWorkspace::World)
            {
                applyWorldRole(descriptor);
                return;
            }

            applyParticleRole(descriptor);
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
                    descriptor.visibleInWorld = true;
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
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 0;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.310f;
                    descriptor.minimumSize = 0.160f;
                    break;

                case ToolPaneId::PropertyInspector:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 1;
                    descriptor.visibleInParticles = true;
                    descriptor.preferredSizeInParticles = 0.500f;
                    break;

                case ToolPaneId::Diagnostics:
                    descriptor.dockArea = ToolDockArea::Right;
                    descriptor.order = 2;
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
                    descriptor.visibleInParticles = true;
                    break;

                default:
                    break;
            }
        }
    };
} // namespace gts::tools
