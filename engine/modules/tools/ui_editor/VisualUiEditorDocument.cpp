#include "VisualUiEditorDocument.h"

#include <algorithm>
#include <utility>

#include "UiSystem.h"

namespace gts::tools
{
    namespace
    {
        constexpr const char* PreviewSurfaceName = "visual-ui-editor.preview";

        EditorPropertyDescriptor makeTextProperty(std::string id,
                                                  std::string name,
                                                  std::string value,
                                                  std::string category,
                                                  bool readOnly = false)
        {
            EditorPropertyDescriptor property;
            property.metadata.id          = std::move(id);
            property.metadata.displayName = std::move(name);
            property.metadata.category    = std::move(category);
            property.metadata.type        = EditorPropertyValueType::Text;
            property.metadata.readOnly    = readOnly;
            property.value.type           = EditorPropertyValueType::Text;
            property.value.textValue      = std::move(value);
            return property;
        }

        UiSerializedWidget* findWidgetRecursive(UiSerializedWidget& widget, const std::string& widgetId)
        {
            if (widget.id == widgetId)
                return &widget;

            for (UiSerializedWidget& child : widget.children)
            {
                if (UiSerializedWidget* found = findWidgetRecursive(child, widgetId))
                    return found;
            }

            for (auto& slot : widget.slots)
            {
                for (UiSerializedWidget& child : slot.second)
                {
                    if (UiSerializedWidget* found = findWidgetRecursive(child, widgetId))
                        return found;
                }
            }

            return nullptr;
        }

        const UiSerializedWidget* findWidgetRecursive(const UiSerializedWidget& widget, const std::string& widgetId)
        {
            if (widget.id == widgetId)
                return &widget;

            for (const UiSerializedWidget& child : widget.children)
            {
                if (const UiSerializedWidget* found = findWidgetRecursive(child, widgetId))
                    return found;
            }

            for (const auto& slot : widget.slots)
            {
                for (const UiSerializedWidget& child : slot.second)
                {
                    if (const UiSerializedWidget* found = findWidgetRecursive(child, widgetId))
                        return found;
                }
            }

            return nullptr;
        }
    }

    bool VisualUiEditorDocument::openAsset(UiWidgetAssetDefinition asset, std::filesystem::path path)
    {
        assetDefinition = std::move(asset);
        sourcePath      = std::move(path);
        open            = !assetDefinition.id.empty();
        dirtyFlag       = false;
        selection       = assetDefinition.root.id;
        return open;
    }

    void VisualUiEditorDocument::close(UiSystem* ui)
    {
        if (ui != nullptr)
            destroyPreview(*ui);

        open = false;
        dirtyFlag = false;
        sourcePath.clear();
        assetDefinition = {};
        selection.clear();
    }

    bool VisualUiEditorDocument::selectWidget(const std::string& widgetId)
    {
        if (!open || findWidget(widgetId) == nullptr)
            return false;

        selection = widgetId;
        return true;
    }

    bool VisualUiEditorDocument::renameSelectedWidget(const std::string& widgetId)
    {
        if (!open || widgetId.empty() || (widgetId != selection && widgetIdExists(widgetId)))
            return false;

        UiSerializedWidget* widget = findWidget(selection);
        if (widget == nullptr)
            return false;

        widget->id = widgetId;
        selection = widgetId;
        markDirty();
        return true;
    }

    bool VisualUiEditorDocument::setSelectedText(const std::string& text)
    {
        UiSerializedWidget* widget = findWidget(selection);
        if (widget == nullptr)
            return false;

        widget->text = text;
        markDirty();
        return true;
    }

    bool VisualUiEditorDocument::setSelectedStyleClass(const std::string& styleClass)
    {
        UiSerializedWidget* widget = findWidget(selection);
        if (widget == nullptr)
            return false;

        widget->styleClass = styleClass;
        markDirty();
        return true;
    }

    bool VisualUiEditorDocument::setSelectedType(const std::string& type)
    {
        UiSerializedWidget* widget = findWidget(selection);
        if (widget == nullptr)
            return false;

        widget->type = type;
        markDirty();
        return true;
    }

    std::vector<VisualUiEditorHierarchyItem> VisualUiEditorDocument::hierarchy() const
    {
        std::vector<VisualUiEditorHierarchyItem> items;
        if (!open || assetDefinition.root.id.empty())
            return items;

        collectHierarchy(assetDefinition.root, items, 0, {});
        return items;
    }

    std::vector<EditorPropertyDescriptor> VisualUiEditorDocument::selectedProperties() const
    {
        std::vector<EditorPropertyDescriptor> properties;
        if (!open)
            return properties;

        properties.push_back(makeTextProperty("asset.id", "Asset", assetDefinition.id, "Document", true));
        properties.push_back(makeTextProperty("asset.displayName", "Display Name", assetDefinition.displayName, "Document", true));
        properties.push_back(makeTextProperty("asset.base", "Base Asset", assetDefinition.baseAsset, "Document", true));

        const UiSerializedWidget* widget = findWidget(selection);
        if (widget == nullptr)
            return properties;

        properties.push_back(makeTextProperty("widget.id", "Widget Id", widget->id, "Selection"));
        properties.push_back(makeTextProperty("widget.type", "Type", widget->type, "Selection"));
        properties.push_back(makeTextProperty("widget.asset", "Asset Ref", widget->asset, "Selection", true));
        properties.push_back(makeTextProperty("widget.variant", "Variant", widget->variant, "Selection"));
        properties.push_back(makeTextProperty("widget.styleClass", "Style Class", widget->styleClass, "Presentation"));
        properties.push_back(makeTextProperty("widget.labelStyleClass", "Label Style", widget->labelStyleClass, "Presentation"));
        properties.push_back(makeTextProperty("widget.text", "Text", widget->text, "Content"));
        return properties;
    }

    UiSerializedValidationResult VisualUiEditorDocument::validate(const UiWidgetAssetRegistry& registry) const
    {
        UiSerializedValidationResult result;
        if (!open)
        {
            result.error("document", "No widget asset is open.");
            return result;
        }

        result = registry.validateAsset(assetDefinition);
        return result;
    }

    bool VisualUiEditorDocument::save(std::string* outError)
    {
        if (sourcePath.empty())
        {
            if (outError != nullptr)
                *outError = "Document does not have a source path.";
            return false;
        }
        return saveAs(sourcePath, outError);
    }

    bool VisualUiEditorDocument::saveAs(const std::filesystem::path& path, std::string* outError)
    {
        if (!open)
        {
            if (outError != nullptr)
                *outError = "No widget asset is open.";
            return false;
        }

        if (!saveUiWidgetAssetToFile(path.string(), assetDefinition, outError))
            return false;

        sourcePath = path;
        dirtyFlag = false;
        return true;
    }

    bool VisualUiEditorDocument::saveAndReload(UiSystem& ui, std::string* outError)
    {
        if (!save(outError))
            return false;

        if (!ui.uiAssets().queueWidgetAssetReload(assetDefinition, sourcePath))
        {
            if (outError != nullptr)
                *outError = "Widget asset reload could not be queued.";
            return false;
        }

        UiAssetReloadResult result = ui.processUiAssetReloads();
        syncPreviewFromLiveConsumer(ui);
        if (!result.success)
        {
            if (outError != nullptr)
                *outError = "Widget asset reload failed validation.";
            return false;
        }
        return true;
    }

    bool VisualUiEditorDocument::saveAsAndReload(UiSystem& ui,
                                                 const std::filesystem::path& path,
                                                 std::string* outError)
    {
        if (!saveAs(path, outError))
            return false;

        if (!ui.uiAssets().queueWidgetAssetReload(assetDefinition, sourcePath))
        {
            if (outError != nullptr)
                *outError = "Widget asset reload could not be queued.";
            return false;
        }

        UiAssetReloadResult result = ui.processUiAssetReloads();
        syncPreviewFromLiveConsumer(ui);
        if (!result.success)
        {
            if (outError != nullptr)
                *outError = "Widget asset reload failed validation.";
            return false;
        }
        return true;
    }

    bool VisualUiEditorDocument::ensurePreviewSurface(UiSystem& ui)
    {
        if (previewState.surface != UI_INVALID_SURFACE && ui.findSurface(previewState.surface) != nullptr)
            return true;

        UiSurfaceDesc desc;
        desc.name         = PreviewSurfaceName;
        desc.kind         = UiSurfaceKind::Screen;
        desc.order        = 900;
        desc.rect         = {0.24f, 0.13f, 0.53f, 0.68f};
        desc.visible      = true;
        desc.enabled      = true;
        desc.inputEnabled = true;
        desc.renderEnabled = true;
        previewState.surface = ui.createSurface(desc);
        if (previewState.surface != UI_INVALID_SURFACE)
            ui.setTheme(previewState.surface, ui.theme());
        return previewState.surface != UI_INVALID_SURFACE;
    }

    bool VisualUiEditorDocument::rebuildPreview(UiSystem& ui)
    {
        previewState.loadResult = {};
        if (!open || !ensurePreviewSurface(ui))
            return false;

        ui.setTheme(previewState.surface, ui.theme());

        if (previewState.mount != UI_INVALID_MOUNT)
        {
            ui.destroyMount(previewState.surface, previewState.mount);
            previewState.mount = UI_INVALID_MOUNT;
        }

        UiSerializedValidationResult registrationValidation;
        if (!ui.widgetAssets().registerAsset(assetDefinition, &registrationValidation))
        {
            previewState.loadResult.validation = registrationValidation;
            return false;
        }

        UiMountDesc mountDesc;
        mountDesc.name = "Visual UI Editor Preview: " + assetDefinition.id;
        previewState.mount = ui.createMount(previewState.surface, mountDesc);
        if (previewState.mount == UI_INVALID_MOUNT)
        {
            previewState.loadResult.validation.error("preview", "Preview mount could not be created.");
            return false;
        }

        UiWidgetAssetInstanceDesc desc;
        desc.assetId = assetDefinition.id;
        previewState.loadResult = ui.instantiateWidgetAsset(previewState.surface, desc, previewState.mount);
        previewState.consumerId = uiWidgetAssetConsumerId(desc, previewState.surface, previewState.mount);
        ++previewState.rebuildCount;
        return previewState.loadResult.success;
    }

    bool VisualUiEditorDocument::syncPreviewFromLiveConsumer(const UiSystem& ui)
    {
        if (previewState.consumerId.empty())
            return false;

        const UiAssetConsumerState* consumer = ui.uiAssets().findConsumer(previewState.consumerId);
        if (consumer == nullptr)
            return false;

        previewState.surface = consumer->desc.surface;
        previewState.mount = consumer->desc.mount;
        previewState.loadResult = consumer->loadResult;
        previewState.rebuildCount = consumer->rebuildCount + 1;
        return true;
    }

    void VisualUiEditorDocument::setPreviewVisible(UiSystem& ui, bool visible)
    {
        if (previewState.surface == UI_INVALID_SURFACE)
            return;

        UiSurface* surface = ui.findSurface(previewState.surface);
        if (surface == nullptr)
            return;

        UiSurfaceDesc desc = surface->desc();
        desc.visible       = visible;
        desc.enabled       = visible;
        desc.inputEnabled  = visible;
        desc.renderEnabled = visible;
        ui.setSurfaceDesc(previewState.surface, desc);
    }

    void VisualUiEditorDocument::destroyPreview(UiSystem& ui)
    {
        if (previewState.mount != UI_INVALID_MOUNT)
        {
            ui.destroyMount(previewState.surface, previewState.mount);
            previewState.mount = UI_INVALID_MOUNT;
        }

        if (previewState.surface != UI_INVALID_SURFACE)
        {
            ui.destroySurface(previewState.surface);
            previewState.surface = UI_INVALID_SURFACE;
        }

        previewState.loadResult = {};
        previewState.consumerId.clear();
    }

    UiSerializedWidget* VisualUiEditorDocument::findWidget(const std::string& widgetId)
    {
        if (!open || widgetId.empty())
            return nullptr;
        return findWidgetRecursive(assetDefinition.root, widgetId);
    }

    const UiSerializedWidget* VisualUiEditorDocument::findWidget(const std::string& widgetId) const
    {
        if (!open || widgetId.empty())
            return nullptr;
        return findWidgetRecursive(assetDefinition.root, widgetId);
    }

    bool VisualUiEditorDocument::widgetIdExists(const std::string& widgetId) const
    {
        return findWidget(widgetId) != nullptr;
    }

    void VisualUiEditorDocument::collectHierarchy(const UiSerializedWidget& widget,
                                                  std::vector<VisualUiEditorHierarchyItem>& items,
                                                  int depth,
                                                  const std::string& slot) const
    {
        VisualUiEditorHierarchyItem item;
        item.widgetId = widget.id;
        item.type     = widget.type.empty() ? (widget.asset.empty() ? "Widget" : "Asset") : widget.type;
        item.asset    = widget.asset;
        item.slot     = slot;
        item.depth    = depth;
        item.selected = widget.id == selection;
        items.push_back(std::move(item));

        for (const UiSerializedWidget& child : widget.children)
            collectHierarchy(child, items, depth + 1, {});

        for (const auto& slotEntry : widget.slots)
        {
            for (const UiSerializedWidget& child : slotEntry.second)
                collectHierarchy(child, items, depth + 1, slotEntry.first);
        }
    }

    void VisualUiEditorDocument::markDirty()
    {
        dirtyFlag = true;
    }
}
