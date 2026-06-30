#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "BitmapFont.h"
#include "IResourceProvider.hpp"
#include "UiCommand.h"
#include "UiDocument.h"
#include "UiFocusManager.h"
#include "UiInputDispatcher.h"
#include "UiInteraction.h"
#include "UiModalManager.h"
#include "UiRenderResolver.h"

// Engine-owned retained UI model plus render-side text/resource bindings.
class UiSystem
{
public:
    struct Metrics
    {
        float    layoutMs        = 0.0f;
        float    visualMs        = 0.0f;
        float    commandBuildMs  = 0.0f;
        uint32_t nodeCount       = 0;
        uint32_t primitiveCount  = 0;
        uint32_t commandCount    = 0;
        uint32_t vertexCount     = 0;
        uint32_t indexCount      = 0;
        uint32_t commandCacheHit = 0;
    };

    explicit UiSystem(IResourceProvider* resources);

    void clear();
    void setEnabled(bool enabled);
    bool isEnabled() const;

    UiHandle getRoot() const;
    UiLayerId createLayer(const std::string& name, int order);
    bool      removeLayer(UiLayerId layerId);
    bool      setLayerOrder(UiLayerId layerId, int order);
    bool      setLayerState(UiLayerId layerId, const UiLayerState& state);
    UiHandle  getLayerRoot(UiLayerId layerId) const;
    UiLayerId getDefaultLayer() const;

    UiHandle createNode(UiNodeType type, UiHandle parent = UI_INVALID_HANDLE);
    bool     removeNode(UiHandle handle);
    bool     reparentNode(UiHandle handle, UiHandle newParent);

    UiNode*       findNode(UiHandle handle);
    const UiNode* findNode(UiHandle handle) const;

    bool setLayout(UiHandle handle, const UiLayoutSpec& layout);
    bool setStyle(UiHandle handle, const UiStyle& style);
    bool setState(UiHandle handle, const UiStateFlags& state);
    bool setPayload(UiHandle handle, const UiNodePayload& payload);
    bool setTextFont(UiHandle handle, BitmapFont* font);

    UiDocument&       getDocument();
    const UiDocument& getDocument() const;
    UiFocusManager&       focusManager();
    const UiFocusManager& focusManager() const;
    UiModalManager&       modalManager();
    const UiModalManager& modalManager() const;
    Metrics           getLastMetrics() const;
    bool              measureText(UiHandle handle, UiTextMeasurement& outMeasurement) const;

    UiInteractionResult     updateInteraction(const UiInputFrame& input);
    UiInteractionResult     getLastInteraction() const;
    const UiDispatchResult& dispatchInput(const UiInputFrame& input, uint64_t frameId = 0);
    const UiDispatchResult& dispatchResult() const;

    UiModalId pushModal(const UiModalDesc& desc);
    bool      popModal(UiModalId modalId,
                       UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    bool      dismissTopModal(UiModalDismissReason reason = UiModalDismissReason::Programmatic);

    UiCommandBuffer extractCommands(int viewportWidth, int viewportHeight);
    const UiCommandBuffer& extractCommandsRef(int viewportWidth, int viewportHeight);

private:
    void removeTextBindingsRecursive(UiHandle handle);

    IResourceProvider*                         resources = nullptr;
    bool                                       enabled   = true;
    UiDocument                                 document;
    UiFocusManager                             focusState;
    UiModalManager                             modalState;
    UiInputDispatcher                          inputDispatcher;
    UiRenderResolver                           resolver;
    std::unordered_map<UiHandle, BitmapFont*>  textBindings;
    uint64_t                                   textBindingRevision = 1;
    UiCommandBuffer                            commandCache;
    UiCommandBuffer                            emptyCommandBuffer;
    uint64_t                                   cachedDocumentRevision = 0;
    uint64_t                                   cachedTextBindingRevision = 0;
    int                                        cachedViewportWidth = 0;
    int                                        cachedViewportHeight = 0;
    bool                                       commandCacheValid = false;
    Metrics                                    lastMetrics;
};
