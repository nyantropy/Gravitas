#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
#include "UiSurface.h"

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

    UiSurfaceId createSurface(const UiSurfaceDesc& desc = {});
    bool        destroySurface(UiSurfaceId surfaceId);
    bool        setSurfaceDesc(UiSurfaceId surfaceId, const UiSurfaceDesc& desc);
    UiSurfaceId getDefaultSurface() const;
    UiSurface*       findSurface(UiSurfaceId surfaceId);
    const UiSurface* findSurface(UiSurfaceId surfaceId) const;
    std::vector<UiSurfaceId> surfaceIds() const;

    UiHandle getRoot() const;
    UiHandle getRoot(UiSurfaceId surfaceId) const;
    UiLayerId createLayer(const std::string& name, int order);
    UiLayerId createLayer(UiSurfaceId surfaceId, const std::string& name, int order);
    bool      removeLayer(UiLayerId layerId);
    bool      removeLayer(UiSurfaceId surfaceId, UiLayerId layerId);
    bool      setLayerOrder(UiLayerId layerId, int order);
    bool      setLayerOrder(UiSurfaceId surfaceId, UiLayerId layerId, int order);
    bool      setLayerState(UiLayerId layerId, const UiLayerState& state);
    bool      setLayerState(UiSurfaceId surfaceId, UiLayerId layerId, const UiLayerState& state);
    UiHandle  getLayerRoot(UiLayerId layerId) const;
    UiHandle  getLayerRoot(UiSurfaceId surfaceId, UiLayerId layerId) const;
    UiLayerId getDefaultLayer() const;
    UiLayerId getDefaultLayer(UiSurfaceId surfaceId) const;

    UiHandle createNode(UiNodeType type, UiHandle parent = UI_INVALID_HANDLE);
    UiHandle createNode(UiSurfaceId surfaceId, UiNodeType type, UiHandle parent = UI_INVALID_HANDLE);
    bool     removeNode(UiHandle handle);
    bool     removeNode(UiSurfaceId surfaceId, UiHandle handle);
    bool     reparentNode(UiHandle handle, UiHandle newParent);
    bool     reparentNode(UiSurfaceId surfaceId, UiHandle handle, UiHandle newParent);

    UiNode*       findNode(UiHandle handle);
    const UiNode* findNode(UiHandle handle) const;
    UiNode*       findNode(UiSurfaceId surfaceId, UiHandle handle);
    const UiNode* findNode(UiSurfaceId surfaceId, UiHandle handle) const;

    bool setLayout(UiHandle handle, const UiLayoutSpec& layout);
    bool setLayout(UiSurfaceId surfaceId, UiHandle handle, const UiLayoutSpec& layout);
    bool setTheme(const UiTheme& theme);
    bool setTheme(UiSurfaceId surfaceId, const UiTheme& theme);
    const UiTheme& theme() const;
    const UiTheme* theme(UiSurfaceId surfaceId) const;
    bool setStyle(UiHandle handle, const UiStyle& style);
    bool setStyle(UiSurfaceId surfaceId, UiHandle handle, const UiStyle& style);
    bool setStyleClass(UiHandle handle, const std::string& styleClass);
    bool setStyleClass(UiSurfaceId surfaceId, UiHandle handle, const std::string& styleClass);
    bool setState(UiHandle handle, const UiStateFlags& state);
    bool setState(UiSurfaceId surfaceId, UiHandle handle, const UiStateFlags& state);
    bool setPayload(UiHandle handle, const UiNodePayload& payload);
    bool setPayload(UiSurfaceId surfaceId, UiHandle handle, const UiNodePayload& payload);
    bool setTextFont(UiHandle handle, BitmapFont* font);
    bool setTextFont(UiSurfaceId surfaceId, UiHandle handle, BitmapFont* font);

    UiDocument&       getDocument();
    const UiDocument& getDocument() const;
    UiDocument*       findDocument(UiSurfaceId surfaceId);
    const UiDocument* findDocument(UiSurfaceId surfaceId) const;
    UiFocusManager&       focusManager();
    const UiFocusManager& focusManager() const;
    UiModalManager&       modalManager();
    const UiModalManager& modalManager() const;
    UiNavigationGraph&       navigationGraph();
    const UiNavigationGraph& navigationGraph() const;
    UiNavigationGraph*       navigationGraph(UiSurfaceId surfaceId);
    const UiNavigationGraph* navigationGraph(UiSurfaceId surfaceId) const;
    bool registerNavigationNode(UiHandle handle, const UiNavigationNodeDesc& desc = {});
    bool registerNavigationNode(UiSurfaceId surfaceId, UiHandle handle, const UiNavigationNodeDesc& desc = {});
    bool unregisterNavigationNode(UiHandle handle);
    bool unregisterNavigationNode(UiSurfaceId surfaceId, UiHandle handle);
    UiDragDropManager&       dragDropManager();
    const UiDragDropManager& dragDropManager() const;
    UiDragDropManager*       dragDropManager(UiSurfaceId surfaceId);
    const UiDragDropManager* dragDropManager(UiSurfaceId surfaceId) const;
    bool registerDragSource(UiHandle handle, const UiDragSourceDesc& desc);
    bool registerDragSource(UiSurfaceId surfaceId, UiHandle handle, const UiDragSourceDesc& desc);
    bool unregisterDragSource(UiHandle handle);
    bool unregisterDragSource(UiSurfaceId surfaceId, UiHandle handle);
    bool registerDropTarget(UiHandle handle, const UiDropTargetDesc& desc);
    bool registerDropTarget(UiSurfaceId surfaceId, UiHandle handle, const UiDropTargetDesc& desc);
    bool unregisterDropTarget(UiHandle handle);
    bool unregisterDropTarget(UiSurfaceId surfaceId, UiHandle handle);
    UiMountManager&       mountManager();
    const UiMountManager& mountManager() const;
    Metrics           getLastMetrics() const;
    bool              measureText(UiHandle handle, UiTextMeasurement& outMeasurement) const;
    bool              measureText(UiSurfaceId surfaceId, UiHandle handle, UiTextMeasurement& outMeasurement) const;

    UiInteractionResult     updateInteraction(const UiInputFrame& input);
    UiInteractionResult     getLastInteraction() const;
    const UiDispatchResult& dispatchInput(const UiInputFrame& input, uint64_t frameId = 0);
    const UiDispatchResult& dispatchResult() const;
    const std::vector<UiEvent>& events() const;
    bool propagateEvent(UiEvent event);

    UiModalId pushModal(const UiModalDesc& desc);
    UiModalId pushModal(UiSurfaceId surfaceId, const UiModalDesc& desc);
    bool      popModal(UiModalId modalId,
                       UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    bool      popModal(UiSurfaceId surfaceId,
                       UiModalId modalId,
                       UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    bool      dismissTopModal(UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    bool      dismissTopModal(UiSurfaceId surfaceId,
                              UiModalDismissReason reason = UiModalDismissReason::Programmatic);

    UiMountId createMount(const UiMountDesc& desc = {});
    UiMountId createMount(UiSurfaceId surfaceId, const UiMountDesc& desc = {});
    bool      destroyMount(UiMountId mountId);
    bool      destroyMount(UiSurfaceId surfaceId, UiMountId mountId);
    bool      attachMount(UiMountId mountId, const UiMountAttachment& attachment);
    bool      attachMount(UiSurfaceId surfaceId, UiMountId mountId, const UiMountAttachment& attachment);
    bool      detachMount(UiMountId mountId);
    bool      detachMount(UiSurfaceId surfaceId, UiMountId mountId);
    UiMount*       findMount(UiMountId mountId);
    const UiMount* findMount(UiMountId mountId) const;
    UiMount*       findMount(UiSurfaceId surfaceId, UiMountId mountId);
    const UiMount* findMount(UiSurfaceId surfaceId, UiMountId mountId) const;
    UiMountId      mountFromNode(UiHandle handle) const;
    UiMountId      mountFromNode(UiSurfaceId surfaceId, UiHandle handle) const;
    UiMountId      rootMount() const;
    UiMountId      rootMount(UiSurfaceId surfaceId) const;
    UiHandle       mountRoot(UiMountId mountId) const;
    UiHandle       mountRoot(UiSurfaceId surfaceId, UiMountId mountId) const;

    UiCompositionId mountComposition(std::unique_ptr<UiComposition> composition,
                                     const UiMountDesc& desc = {});
    UiCompositionId mountComposition(UiSurfaceId surfaceId,
                                     std::unique_ptr<UiComposition> composition,
                                     const UiMountDesc& desc = {});
    UiCompositionId attachComposition(UiMountId mountId, std::unique_ptr<UiComposition> composition);
    UiCompositionId attachComposition(UiSurfaceId surfaceId,
                                      UiMountId mountId,
                                      std::unique_ptr<UiComposition> composition);
    bool            updateComposition(UiCompositionId compositionId);
    void            updateCompositions();
    bool            rebuildComposition(UiCompositionId compositionId);
    bool            destroyComposition(UiCompositionId compositionId);
    UiComposition*       findComposition(UiCompositionId compositionId);
    const UiComposition* findComposition(UiCompositionId compositionId) const;
    UiCompositionId      compositionFromMount(UiMountId mountId) const;
    UiCompositionId      compositionFromMount(UiSurfaceId surfaceId, UiMountId mountId) const;
    UiMountId            compositionMount(UiCompositionId compositionId) const;
    UiSurfaceId          compositionSurface(UiCompositionId compositionId) const;

    UiCommandBuffer extractCommands(int viewportWidth, int viewportHeight);
    const UiCommandBuffer& extractCommandsRef(int viewportWidth, int viewportHeight);
    const UiCommandBuffer& extractSurfaceCommandsRef(UiSurfaceId surfaceId,
                                                     int viewportWidth,
                                                     int viewportHeight);

private:
    struct SurfaceRecord
    {
        UiSurface surface;
        std::unordered_map<UiHandle, BitmapFont*> textBindings;
        uint64_t textBindingRevision = 1;
        UiCommandBuffer commandCache;
        bool commandCacheValid = false;
        uint64_t cachedDocumentRevision = 0;
        uint64_t cachedTextBindingRevision = 0;
        int cachedViewportWidth = 0;
        int cachedViewportHeight = 0;
        uint64_t lastPropagatedDispatchSequence = 0;
        Metrics lastMetrics;
    };

    struct MountedComposition
    {
        UiCompositionId id = UI_INVALID_COMPOSITION;
        UiSurfaceId surface = UI_INVALID_SURFACE;
        UiMountId mount = UI_INVALID_MOUNT;
        std::unique_ptr<UiComposition> composition;
    };

    SurfaceRecord*       findSurfaceRecord(UiSurfaceId surfaceId);
    const SurfaceRecord* findSurfaceRecord(UiSurfaceId surfaceId) const;
    SurfaceRecord&       defaultSurfaceRecord();
    const SurfaceRecord& defaultSurfaceRecord() const;
    UiSurfaceId          selectInputSurface(const UiInputFrame& input) const;
    void                 sortSurfaces();
    void                 recreateDefaultSurface();
    static uint64_t      mountCompositionKey(UiSurfaceId surfaceId, UiMountId mountId);

    void removeTextBindingsRecursive(SurfaceRecord& record, UiHandle handle);
    UiCompositionContext makeCompositionContext(UiSurfaceId surfaceId, UiMountId mountId);
    void destroyCompositionRecordsForMount(UiSurfaceId surfaceId, UiMountId mountId);
    void destroyCompositionRecordsForSurface(UiSurfaceId surfaceId);
    void destroyAllCompositionRecords();
    void propagateDispatchEvents(SurfaceRecord& record, uint64_t dispatchSequence);
    bool routeEvent(UiEvent& event);
    bool deliverEventToCurrentTarget(UiEvent& event);
    void resolveEventTarget(UiEvent& event) const;
    void assignCurrentEventTarget(UiEvent& event, UiHandle currentTarget) const;
    void appendSurfaceCommands(UiCommandBuffer& destination,
                               const UiCommandBuffer& source,
                               const UiRect& surfaceRect) const;

    IResourceProvider*                         resources = nullptr;
    bool                                       enabled   = true;
    UiRenderResolver                           resolver;
    std::unordered_map<UiSurfaceId, SurfaceRecord> surfaces;
    std::vector<UiSurfaceId>                   orderedSurfaces;
    std::unordered_map<UiCompositionId, MountedComposition> compositions;
    std::unordered_map<uint64_t, UiCompositionId> mountToComposition;
    std::vector<UiEvent>                       lastEvents;
    UiSurfaceId                                defaultSurfaceId = UI_DEFAULT_SURFACE;
    UiSurfaceId                                nextSurfaceId = UI_DEFAULT_SURFACE + 1;
    UiCompositionId                            nextCompositionId = UI_INVALID_COMPOSITION + 1;
    UiDispatchResult                           lastDispatchResult;
    UiCommandBuffer                            commandCache;
    UiCommandBuffer                            emptyCommandBuffer;
    Metrics                                    lastMetrics;
};
