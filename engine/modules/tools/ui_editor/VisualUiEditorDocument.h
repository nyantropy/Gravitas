#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "EditorPropertySystem.h"
#include "UiSerialization.h"
#include "UiSurface.h"
#include "UiWidgetAsset.h"

class UiSystem;

namespace gts::tools
{
    struct VisualUiEditorHierarchyItem
    {
        std::string widgetId;
        std::string type;
        std::string asset;
        std::string slot;
        int         depth = 0;
        bool        selected = false;
    };

    struct VisualUiEditorPreviewState
    {
        UiSurfaceId surface = UI_INVALID_SURFACE;
        UiMountId mount = UI_INVALID_MOUNT;
        UiSerializedLoadResult loadResult;
        uint32_t rebuildCount = 0;
    };

    class VisualUiEditorDocument
    {
    public:
        bool openAsset(UiWidgetAssetDefinition asset, std::filesystem::path sourcePath = {});
        void close(UiSystem* ui = nullptr);

        bool hasAsset() const { return open; }
        bool dirty() const { return dirtyFlag; }
        const std::filesystem::path& path() const { return sourcePath; }
        const UiWidgetAssetDefinition& asset() const { return assetDefinition; }
        UiWidgetAssetDefinition& asset() { return assetDefinition; }

        const std::string& selectedWidgetId() const { return selection; }
        bool selectWidget(const std::string& widgetId);
        bool renameSelectedWidget(const std::string& widgetId);
        bool setSelectedText(const std::string& text);
        bool setSelectedStyleClass(const std::string& styleClass);
        bool setSelectedType(const std::string& type);

        std::vector<VisualUiEditorHierarchyItem> hierarchy() const;
        std::vector<EditorPropertyDescriptor> selectedProperties() const;
        UiSerializedValidationResult validate(const UiWidgetAssetRegistry& registry) const;

        bool save(std::string* outError = nullptr);
        bool saveAs(const std::filesystem::path& path, std::string* outError = nullptr);

        bool ensurePreviewSurface(UiSystem& ui);
        bool rebuildPreview(UiSystem& ui);
        void setPreviewVisible(UiSystem& ui, bool visible);
        void destroyPreview(UiSystem& ui);
        const VisualUiEditorPreviewState& preview() const { return previewState; }

    private:
        UiSerializedWidget* findWidget(const std::string& widgetId);
        const UiSerializedWidget* findWidget(const std::string& widgetId) const;
        bool widgetIdExists(const std::string& widgetId) const;
        void collectHierarchy(const UiSerializedWidget& widget,
                              std::vector<VisualUiEditorHierarchyItem>& items,
                              int depth,
                              const std::string& slot) const;
        void markDirty();

        bool open = false;
        bool dirtyFlag = false;
        std::filesystem::path sourcePath;
        UiWidgetAssetDefinition assetDefinition;
        std::string selection;
        VisualUiEditorPreviewState previewState;
    };
}
