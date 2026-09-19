#pragma once

#include <unordered_map>
#include <vector>

#include "UiDocument.h"
#include "UiFocusManager.h"
#include "UiModalManager.h"
#include "UiNavigationTypes.h"

struct UiNavigationNode
{
    UiHandle handle = UI_INVALID_HANDLE;
    UiNavigationNodeDesc desc;
    uint64_t registrationOrder = 0;
};

class UiNavigationGraph
{
public:
    void clear();

    bool registerNode(UiHandle handle, const UiNavigationNodeDesc& desc = {});
    bool unregisterNode(UiHandle handle);
    void unregisterSubtree(const UiDocument& document, UiHandle root);
    void pruneInvalidNodes(const UiDocument& document);

    bool setNeighbor(UiHandle handle, UiNavigationDirection direction, UiHandle neighbor);
    bool setNodeEnabled(UiHandle handle, bool enabled);

    const UiNavigationNode* findNode(UiHandle handle) const;
    std::vector<UiHandle> navigationOrder(const UiDocument& document,
                                          const UiFocusManager& focusManager,
                                          const UiModalManager& modalManager,
                                          UiHandle current = UI_INVALID_HANDLE) const;

    UiNavigationMoveResult move(UiDocument& document,
                                UiFocusManager& focusManager,
                                const UiModalManager& modalManager,
                                UiNavigationDirection direction,
                                UiInputDeviceId deviceId = UI_PRIMARY_INPUT_DEVICE);
    UiNavigationSubmitResult submit(UiDocument& document,
                                    UiFocusManager& focusManager,
                                    const UiModalManager& modalManager,
                                    UiInputDeviceId deviceId = UI_PRIMARY_INPUT_DEVICE);

private:
    struct Candidate
    {
        UiHandle handle = UI_INVALID_HANDLE;
        UiRect bounds;
        UiNavigationNode node;
    };

    std::vector<Candidate> candidates(const UiDocument& document,
                                      const UiFocusManager& focusManager,
                                      const UiModalManager& modalManager,
                                      UiHandle current = UI_INVALID_HANDLE) const;
    bool isNodeAvailable(const UiDocument& document,
                         const UiFocusManager& focusManager,
                         const UiModalManager& modalManager,
                         const UiNavigationNode& node,
                         const std::string& currentGroup) const;
    bool isNodeVisibleAndEnabled(const UiDocument& document, UiHandle handle) const;
    UiHandle currentNavigationTarget(const UiDocument& document,
                                     const UiFocusManager& focusManager,
                                     const UiModalManager& modalManager,
                                     UiInputDeviceId deviceId) const;
    UiHandle resolveExplicitNeighbor(const UiDocument& document,
                                     const UiFocusManager& focusManager,
                                     const UiModalManager& modalManager,
                                     UiHandle current,
                                     UiNavigationDirection direction) const;
    UiHandle resolveLinear(const std::vector<Candidate>& candidates,
                           UiHandle current,
                           UiNavigationDirection direction,
                           bool wrap,
                           bool& wrapped) const;
    UiHandle resolveDirectional(const std::vector<Candidate>& candidates,
                                UiHandle current,
                                UiNavigationDirection direction,
                                bool wrap,
                                bool& wrapped) const;
    static bool isForwardDirection(UiNavigationDirection direction);
    static float centerX(const UiRect& rect);
    static float centerY(const UiRect& rect);

    std::unordered_map<UiHandle, UiNavigationNode> nodes;
    uint64_t nextRegistrationOrder = 1;
};
