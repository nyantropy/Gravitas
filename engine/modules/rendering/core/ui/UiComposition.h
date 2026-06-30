#pragma once

#include "IResourceProvider.hpp"
#include "UiCompositionTypes.h"
#include "UiDocument.h"
#include "UiEvent.h"
#include "UiMountTypes.h"

class UiSystem;

struct UiCompositionContext
{
    UiSystem& ui;
    UiDocument& document;
    IResourceProvider* resources = nullptr;
    UiMountId mount = UI_INVALID_MOUNT;
    UiHandle root = UI_INVALID_HANDLE;
};

class UiComposition
{
public:
    virtual ~UiComposition() = default;

    virtual void build(UiCompositionContext& context) = 0;
    virtual void update(UiCompositionContext& context) {}
    virtual void onEvent(UiCompositionContext& context, UiEvent& event) {}
    virtual void destroy(UiCompositionContext& context) {}
};
