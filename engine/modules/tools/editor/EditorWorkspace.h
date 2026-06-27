#pragma once

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "EditorLayout.h"

namespace gts::tools
{
    struct EditorWorkspacePanelLayout
    {
        std::string    panelId;
        EditorDockArea area      = EditorDockArea::Center;
        float          size      = 0.25f;
        bool           visible   = true;
        bool           collapsed = false;
        std::string    selectedTabId;
    };

    struct EditorWorkspaceToolbarItem
    {
        std::string commandId;
        bool        visible = true;
        int         order   = 0;
    };

    struct EditorWorkspaceState
    {
        std::string                             id;
        std::string                             displayName;
        std::string                             selectedWorkspaceTabId;
        std::vector<EditorWorkspacePanelLayout> panels;
        std::vector<EditorWorkspaceToolbarItem> toolbarItems;
    };

    class EditorWorkspaceRegistry
    {
        public:
        void addWorkspace(EditorWorkspaceState workspace)
        {
            if (activeWorkspaceId.empty())
                activeWorkspaceId = workspace.id;
            workspaces.push_back(std::move(workspace));
        }

        EditorWorkspaceState* findWorkspace(const std::string& id)
        {
            for (EditorWorkspaceState& workspace : workspaces)
            {
                if (workspace.id == id)
                    return &workspace;
            }
            return nullptr;
        }

        const EditorWorkspaceState* findWorkspace(const std::string& id) const
        {
            for (const EditorWorkspaceState& workspace : workspaces)
            {
                if (workspace.id == id)
                    return &workspace;
            }
            return nullptr;
        }

        bool setActiveWorkspace(const std::string& id)
        {
            if (findWorkspace(id) == nullptr)
                return false;
            activeWorkspaceId = id;
            return true;
        }

        const EditorWorkspaceState* activeWorkspace() const
        {
            return findWorkspace(activeWorkspaceId);
        }

        const std::string& activeId() const
        {
            return activeWorkspaceId;
        }

        const std::vector<EditorWorkspaceState>& all() const
        {
            return workspaces;
        }

        private:
        std::vector<EditorWorkspaceState> workspaces;
        std::string                       activeWorkspaceId;
    };

    inline int dockAreaToInt(EditorDockArea area)
    {
        switch (area)
        {
        case EditorDockArea::Center:
            return 0;
        case EditorDockArea::Left:
            return 1;
        case EditorDockArea::Right:
            return 2;
        case EditorDockArea::Top:
            return 3;
        case EditorDockArea::Bottom:
            return 4;
        }
        return 0;
    }

    inline EditorDockArea dockAreaFromInt(int value)
    {
        switch (value)
        {
        case 1:
            return EditorDockArea::Left;
        case 2:
            return EditorDockArea::Right;
        case 3:
            return EditorDockArea::Top;
        case 4:
            return EditorDockArea::Bottom;
        case 0:
        default:
            return EditorDockArea::Center;
        }
    }

    inline EditorWorkspaceState makeDefaultWorkspace(std::string id, std::string displayName)
    {
        EditorWorkspaceState workspace;
        workspace.id                     = std::move(id);
        workspace.displayName            = std::move(displayName);
        workspace.selectedWorkspaceTabId = "main";
        workspace.panels                 = {{"hierarchy", EditorDockArea::Left, 0.22f, true, false, ""},
                                            {"inspector", EditorDockArea::Right, 0.26f, true, false, ""},
                                            {"workspace", EditorDockArea::Bottom, 0.24f, true, false, "main"}};
        workspace.toolbarItems           = {{"editor.save", true, 0},
                                            {"editor.reload", true, 1},
                                            {"editor.undo", true, 2},
                                            {"editor.redo", true, 3},
                                            {"editor.frame_selection", true, 4}};
        return workspace;
    }

    inline std::vector<EditorWorkspaceState> makeDefaultEditorWorkspaces()
    {
        return {makeDefaultWorkspace("scene", "Scene Editing"),
                makeDefaultWorkspace("fx", "FX Editing"),
                makeDefaultWorkspace("animation", "Animation"),
                makeDefaultWorkspace("materials", "Materials")};
    }

    inline std::string serializeWorkspace(const EditorWorkspaceState& workspace)
    {
        std::ostringstream out;
        out << "workspace " << std::quoted(workspace.id) << ' ' << std::quoted(workspace.displayName) << ' '
            << std::quoted(workspace.selectedWorkspaceTabId) << '\n';

        for (const EditorWorkspacePanelLayout& panel : workspace.panels)
        {
            out << "panel " << std::quoted(panel.panelId) << ' ' << dockAreaToInt(panel.area) << ' ' << panel.size
                << ' ' << (panel.visible ? 1 : 0) << ' ' << (panel.collapsed ? 1 : 0) << ' '
                << std::quoted(panel.selectedTabId) << '\n';
        }

        for (const EditorWorkspaceToolbarItem& item : workspace.toolbarItems)
        {
            out << "toolbar " << std::quoted(item.commandId) << ' ' << (item.visible ? 1 : 0) << ' ' << item.order
                << '\n';
        }
        return out.str();
    }

    inline bool
    deserializeWorkspace(const std::string& serialized, EditorWorkspaceState& workspace, std::string* error = nullptr)
    {
        EditorWorkspaceState parsed;
        std::istringstream   input(serialized);
        std::string          line;
        bool                 foundWorkspace = false;

        while (std::getline(input, line))
        {
            if (line.empty())
                continue;

            std::istringstream row(line);
            std::string        tag;
            row >> tag;

            if (tag == "workspace")
            {
                row >> std::quoted(parsed.id) >> std::quoted(parsed.displayName) >>
                    std::quoted(parsed.selectedWorkspaceTabId);
                foundWorkspace = !parsed.id.empty();
            }
            else if (tag == "panel")
            {
                EditorWorkspacePanelLayout panel;
                int                        area      = 0;
                int                        visible   = 1;
                int                        collapsed = 0;
                row >> std::quoted(panel.panelId) >> area >> panel.size >> visible >> collapsed >>
                    std::quoted(panel.selectedTabId);
                panel.area      = dockAreaFromInt(area);
                panel.visible   = visible != 0;
                panel.collapsed = collapsed != 0;
                if (!panel.panelId.empty())
                    parsed.panels.push_back(panel);
            }
            else if (tag == "toolbar")
            {
                EditorWorkspaceToolbarItem item;
                int                        visible = 1;
                row >> std::quoted(item.commandId) >> visible >> item.order;
                item.visible = visible != 0;
                if (!item.commandId.empty())
                    parsed.toolbarItems.push_back(item);
            }
        }

        if (!foundWorkspace)
        {
            if (error != nullptr)
                *error = "workspace header missing";
            return false;
        }

        workspace = std::move(parsed);
        return true;
    }

    inline bool saveWorkspaceFile(const std::filesystem::path& path,
                                  const EditorWorkspaceState&  workspace,
                                  std::string*                 error = nullptr)
    {
        std::ofstream output(path);
        if (!output)
        {
            if (error != nullptr)
                *error = "failed to open workspace file for writing";
            return false;
        }

        output << serializeWorkspace(workspace);
        return true;
    }

    inline bool
    loadWorkspaceFile(const std::filesystem::path& path, EditorWorkspaceState& workspace, std::string* error = nullptr)
    {
        std::ifstream input(path);
        if (!input)
        {
            if (error != nullptr)
                *error = "failed to open workspace file for reading";
            return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        return deserializeWorkspace(buffer.str(), workspace, error);
    }

    inline std::vector<EditorPanelState> workspacePanelsToLayout(const EditorWorkspaceState& workspace)
    {
        std::vector<EditorPanelState> panels;
        panels.reserve(workspace.panels.size());
        for (const EditorWorkspacePanelLayout& layout : workspace.panels)
        {
            EditorPanelState panel;
            panel.id           = layout.panelId;
            panel.title        = layout.panelId;
            panel.area         = layout.area;
            panel.size         = layout.size;
            panel.restoredSize = layout.size;
            panel.visible      = layout.visible;
            panel.collapsed    = layout.collapsed;
            panels.push_back(panel);
        }
        return panels;
    }
} // namespace gts::tools
