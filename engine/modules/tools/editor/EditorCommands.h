#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gts::tools
{
    namespace EditorStandardCommands
    {
        inline constexpr const char* Undo           = "editor.undo";
        inline constexpr const char* Redo           = "editor.redo";
        inline constexpr const char* Copy           = "editor.copy";
        inline constexpr const char* Paste          = "editor.paste";
        inline constexpr const char* Duplicate      = "editor.duplicate";
        inline constexpr const char* Delete         = "editor.delete";
        inline constexpr const char* Rename         = "editor.rename";
        inline constexpr const char* FrameSelection = "editor.frame_selection";
        inline constexpr const char* ResetCamera    = "editor.reset_camera";
        inline constexpr const char* Save           = "editor.save";
        inline constexpr const char* Reload         = "editor.reload";
    } // namespace EditorStandardCommands

    struct EditorShortcut
    {
        std::string action;
        std::string displayText;
    };

    struct EditorCommandContext
    {
        std::string workspaceId;
        std::string focusedWidgetId;
        std::string selectionScope;
        bool        textEditing = false;
    };

    struct EditorCommandResult
    {
        bool        handled = false;
        std::string status;
    };

    struct EditorCommandDefinition
    {
        std::string    id;
        std::string    label;
        std::string    description;
        EditorShortcut shortcut;
        bool           enabled                 = true;
        bool           checkable               = false;
        bool           checked                 = false;
        bool           allowedWhileTextEditing = false;
    };

    using EditorCommandHandler = std::function<EditorCommandResult(const EditorCommandContext&)>;

    class EditorCommandRegistry
    {
        public:
        void registerCommand(EditorCommandDefinition definition, EditorCommandHandler handler = {})
        {
            const std::string id             = definition.id;
            const std::string shortcutAction = definition.shortcut.action;
            commands[id]                     = {std::move(definition), std::move(handler)};
            if (!shortcutAction.empty())
                shortcuts[shortcutAction] = id;
        }

        bool hasCommand(const std::string& id) const
        {
            return commands.find(id) != commands.end();
        }

        const EditorCommandDefinition* findCommand(const std::string& id) const
        {
            const auto it = commands.find(id);
            return it == commands.end() ? nullptr : &it->second.definition;
        }

        bool setEnabled(const std::string& id, bool enabled)
        {
            auto it = commands.find(id);
            if (it == commands.end())
                return false;
            it->second.definition.enabled = enabled;
            return true;
        }

        bool setChecked(const std::string& id, bool checked)
        {
            auto it = commands.find(id);
            if (it == commands.end())
                return false;
            it->second.definition.checked = checked;
            return true;
        }

        bool setHandler(const std::string& id, EditorCommandHandler handler)
        {
            auto it = commands.find(id);
            if (it == commands.end())
                return false;
            it->second.handler = std::move(handler);
            return true;
        }

        const EditorCommandDefinition* commandForShortcut(const std::string& action) const
        {
            const auto shortcutIt = shortcuts.find(action);
            if (shortcutIt == shortcuts.end())
                return nullptr;
            return findCommand(shortcutIt->second);
        }

        EditorCommandResult execute(const std::string& id, const EditorCommandContext& context) const
        {
            const auto it = commands.find(id);
            if (it == commands.end())
                return {false, "unknown command"};

            const RegisteredCommand& command = it->second;
            if (!command.definition.enabled)
                return {false, "command disabled"};
            if (context.textEditing && !command.definition.allowedWhileTextEditing)
                return {false, "command blocked by text editing"};
            if (!command.handler)
                return {false, "command has no handler"};
            return command.handler(context);
        }

        EditorCommandResult routeShortcut(const std::string& action, const EditorCommandContext& context) const
        {
            const EditorCommandDefinition* command = commandForShortcut(action);
            if (command == nullptr)
                return {false, "shortcut not mapped"};
            return execute(command->id, context);
        }

        std::vector<EditorCommandDefinition> listCommands() const
        {
            std::vector<EditorCommandDefinition> result;
            result.reserve(commands.size());
            for (const auto& entry : commands)
                result.push_back(entry.second.definition);
            return result;
        }

        private:
        struct RegisteredCommand
        {
            EditorCommandDefinition definition;
            EditorCommandHandler    handler;
        };

        std::unordered_map<std::string, RegisteredCommand> commands;
        std::unordered_map<std::string, std::string>       shortcuts;
    };

    struct EditorUndoRecord
    {
        std::string           label;
        std::function<void()> undo;
        std::function<void()> redo;
    };

    class EditorUndoStack
    {
        public:
        explicit EditorUndoStack(size_t maxRecords = 128) : maxRecordCount(maxRecords) {}

        void push(EditorUndoRecord record)
        {
            if (cursor < records.size())
                records.erase(records.begin() + static_cast<std::ptrdiff_t>(cursor), records.end());

            records.push_back(std::move(record));
            if (records.size() > maxRecordCount)
                records.erase(records.begin());
            cursor = records.size();
        }

        bool canUndo() const
        {
            return cursor > 0 && cursor <= records.size();
        }

        bool canRedo() const
        {
            return cursor < records.size();
        }

        std::string undoLabel() const
        {
            return canUndo() ? records[cursor - 1].label : std::string{};
        }

        std::string redoLabel() const
        {
            return canRedo() ? records[cursor].label : std::string{};
        }

        bool undo()
        {
            if (!canUndo())
                return false;

            --cursor;
            if (records[cursor].undo)
                records[cursor].undo();
            return true;
        }

        bool redo()
        {
            if (!canRedo())
                return false;

            if (records[cursor].redo)
                records[cursor].redo();
            ++cursor;
            return true;
        }

        size_t size() const
        {
            return records.size();
        }

        private:
        std::vector<EditorUndoRecord> records;
        size_t                        cursor         = 0;
        size_t                        maxRecordCount = 128;
    };

    inline void registerStandardEditorCommands(EditorCommandRegistry& registry)
    {
        registry.registerCommand({EditorStandardCommands::Undo,
                                  "Undo",
                                  "Undo the last editor operation.",
                                  {"editor.shortcut.undo", "Ctrl+Z"},
                                  true,
                                  false,
                                  false,
                                  true});
        registry.registerCommand({EditorStandardCommands::Redo,
                                  "Redo",
                                  "Redo the next editor operation.",
                                  {"editor.shortcut.redo", "Ctrl+Y"},
                                  true,
                                  false,
                                  false,
                                  true});
        registry.registerCommand({EditorStandardCommands::Copy,
                                  "Copy",
                                  "Copy the current selection.",
                                  {"editor.shortcut.copy", "Ctrl+C"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::Paste,
                                  "Paste",
                                  "Paste into the current selection.",
                                  {"editor.shortcut.paste", "Ctrl+V"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::Duplicate,
                                  "Duplicate",
                                  "Duplicate the current selection.",
                                  {"editor.shortcut.duplicate", "Ctrl+D"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::Delete,
                                  "Delete",
                                  "Delete the current selection.",
                                  {"editor.shortcut.delete", "Del"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::Rename,
                                  "Rename",
                                  "Rename the current selection.",
                                  {"editor.shortcut.rename", "F2"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::FrameSelection,
                                  "Frame Selection",
                                  "Frame the active viewport around the selection.",
                                  {"editor.shortcut.frame_selection", "F"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::ResetCamera,
                                  "Reset Camera",
                                  "Reset the active editor camera.",
                                  {"editor.shortcut.reset_camera", "Home"},
                                  true,
                                  false,
                                  false,
                                  false});
        registry.registerCommand({EditorStandardCommands::Save,
                                  "Save",
                                  "Save the active editor document.",
                                  {"editor.shortcut.save", "Ctrl+S"},
                                  true,
                                  false,
                                  false,
                                  true});
        registry.registerCommand({EditorStandardCommands::Reload,
                                  "Reload",
                                  "Reload the active editor document.",
                                  {"editor.shortcut.reload", "Ctrl+R"},
                                  true,
                                  false,
                                  false,
                                  false});
    }
} // namespace gts::tools
