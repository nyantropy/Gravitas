#pragma once

#include <utility>

#include "UiComposition.h"
#include "VNDialogueUi.h"
#include "VNRuntime.h"

namespace gts::vn
{
    class VNDialogueComposition : public UiComposition
    {
    public:
        VNDialogueComposition(VNDialogueUiConfig inConfig, VNRuntime& inRuntime)
            : config(std::move(inConfig)),
              runtime(inRuntime)
        {
        }

        void build(UiCompositionContext& context) override
        {
            handles = VNDialogueUi::build(context.ui, context.root, config);
        }

        void update(UiCompositionContext& context) override
        {
            VNDialogueUi::sync(context.ui, handles, config, runtime);
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            if (event.phase != UiEventPhase::Target)
                return;

            const int choiceIndex = VNDialogueUi::handleChoiceEvent(context.ui, handles, event);
            if (choiceIndex < 0)
                return;

            clickedChoiceIndex = choiceIndex;
            event.consume();
            event.preventDefault();
        }

        void destroy(UiCompositionContext& context) override
        {
            VNDialogueUi::destroy(context.ui, handles);
        }

        int consumeClickedChoiceIndex()
        {
            const int result = clickedChoiceIndex;
            clickedChoiceIndex = -1;
            return result;
        }

    private:
        VNDialogueUiConfig config;
        VNRuntime& runtime;
        VNDialogueUiHandles handles;
        int clickedChoiceIndex = -1;
    };
} // namespace gts::vn
