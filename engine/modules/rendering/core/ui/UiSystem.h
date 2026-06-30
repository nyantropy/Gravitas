#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "BitmapFont.h"
#include "IResourceProvider.hpp"
#include "UiCommand.h"
#include "UiComposition.h"
#include "UiDocument.h"
#include "UiFocusManager.h"
#include "UiInputDispatcher.h"
#include "UiInteraction.h"
#include "UiModalManager.h"
#include "UiMount.h"
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
    UiMountManager&       mountManager();
    const UiMountManager& mountManager() const;
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

    UiMountId createMount(const UiMountDesc& desc = {});
    bool      destroyMount(UiMountId mountId);
    bool      attachMount(UiMountId mountId, const UiMountAttachment& attachment);
    bool      detachMount(UiMountId mountId);
    UiMount*       findMount(UiMountId mountId);
    const UiMount* findMount(UiMountId mountId) const;
    UiMountId      mountFromNode(UiHandle handle) const;
    UiMountId      rootMount() const;
    UiHandle       mountRoot(UiMountId mountId) const;

    UiCompositionId mountComposition(std::unique_ptr<UiComposition> composition,
                                     const UiMountDesc& desc = {});
    UiCompositionId attachComposition(UiMountId mountId, std::unique_ptr<UiComposition> composition);
    bool            updateComposition(UiCompositionId compositionId);
    void            updateCompositions();
    bool            rebuildComposition(UiCompositionId compositionId);
    bool            destroyComposition(UiCompositionId compositionId);
    UiComposition*       findComposition(UiCompositionId compositionId);
    const UiComposition* findComposition(UiCompositionId compositionId) const;
    UiCompositionId      compositionFromMount(UiMountId mountId) const;
    UiMountId            compositionMount(UiCompositionId compositionId) const;

    UiCommandBuffer extractCommands(int viewportWidth, int viewportHeight);
    const UiCommandBuffer& extractCommandsRef(int viewportWidth, int viewportHeight);

private:
    struct MountedComposition
    {
        UiCompositionId id = UI_INVALID_COMPOSITION;
        UiMountId mount = UI_INVALID_MOUNT;
        std::unique_ptr<UiComposition> composition;
    };

    void removeTextBindingsRecursive(UiHandle handle);
    UiCompositionContext makeCompositionContext(UiMountId mountId);
    void destroyCompositionRecordsForMount(UiMountId mountId);
    void destroyAllCompositionRecords();

    IResourceProvider*                         resources = nullptr;
    bool                                       enabled   = true;
    UiDocument                                 document;
    UiFocusManager                             focusState;
    UiModalManager                             modalState;
    UiMountManager                             mountState;
    UiInputDispatcher                          inputDispatcher;
    UiRenderResolver                           resolver;
    std::unordered_map<UiHandle, BitmapFont*>  textBindings;
    std::unordered_map<UiCompositionId, MountedComposition> compositions;
    std::unordered_map<UiMountId, UiCompositionId> mountToComposition;
    UiCompositionId                            nextCompositionId = UI_INVALID_COMPOSITION + 1;
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
